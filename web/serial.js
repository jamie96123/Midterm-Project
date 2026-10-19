// Newline framing must tolerate arbitrary USB packet boundaries and reconnects.
export class LineDecoder {
  constructor(onLine, onError, maxLength=2048) { this.onLine=onLine; this.onError=onError; this.maxLength=maxLength; this.buffer=''; this.discard=false; }
  feed(text) {
    for (const c of text) {
      if (c==='\n') {
        if (!this.discard && this.buffer.trim()) this.onLine(this.buffer.trim());
        this.buffer=''; this.discard=false;
      } else if (!this.discard) {
        if (this.buffer.length>=this.maxLength) { this.buffer=''; this.discard=true; this.onError('接收訊息過長，已捨棄此行。'); }
        else this.buffer+=c;
      }
    }
  }
}
export function validateState(g) {
  if (!g || g.t!=='state' || g.v!==1 || !['ready','running','paused','finished'].includes(g.state)) return false;
  if (typeof g.word!=='string' || !/^[a-z]{0,23}$/.test(g.word) || !['green','yellow','red','blue'].includes(g.color)) return false;
  const keys=['pos','score','correct','wrong','misses','attempts','hits','question','total','remaining','duration','elapsed','rate','manual','lastAward','rxErrors','txDrops','protocolErrors'];
  if (!keys.every(k=>Number.isSafeInteger(g[k]) && g[k]>=0)) return false;
  return g.total===10 && g.question<=10 && g.pos<=g.word.length && g.rate<=2 && g.manual<=1 && g.remaining<=g.duration && [3000,5000,8000].includes(g.duration);
}
export class SerialLink {
  constructor(onState, onLog, onClose) {
    this.onState=onState; this.onLog=onLog; this.onClose=onClose;
    this.port=null; this.reader=null; this.writer=null; this.readTask=null; this.closing=false; this.pending=0;
  }
  async connect(settings) {
    if (!navigator.serial) throw new Error('請使用電腦版 Chrome 或 Edge，並以 localhost 或 HTTPS 開啟。');
    const port=await navigator.serial.requestPort();
    await port.open({...settings, bufferSize:4096});
    this.port=port; this.closing=false;
    this.writer=port.writable.getWriter();
    this.readTask=this.readLoop();
  }
  async readLoop() {
    const decoder=new TextDecoder();
    const lines=new LineDecoder(line=>{
      this.onLog('RX',line);
      try {
        const g=JSON.parse(line);
        if (g.t==='error') this.onLog('ERROR',`板端回報：${g.code}`);
        else if (validateState(g)) this.onState(g);
        else this.onLog('ERROR','無法辨識狀態，請確認韌體為 TypeRush v1。');
      } catch { this.onLog('ERROR','非 JSON 訊息，請確認鮑率與韌體。'); }
    }, message=>this.onLog('ERROR',message));
    try {
      this.reader=this.port.readable.getReader();
      while (!this.closing) {
        const {value,done}=await this.reader.read();
        if (done) break;
        lines.feed(decoder.decode(value,{stream:true}));
      }
    } catch (e) { if (!this.closing) this.onLog('ERROR',`序列埠中斷：${e.message}`); }
    finally {
      this.reader?.releaseLock(); this.reader=null;
      // Defer until the read task has resolved; cleanup can then safely await it.
      if (!this.closing) queueMicrotask(()=>this.disconnect());
    }
  }
  async send(command) {
    if (!this.writer || this.closing) throw new Error('尚未連接開發板。');
    if (this.pending>=32) throw new Error('傳送佇列已滿，請放慢輸入並確認連線。');
    if (!/^(SYNC|START|PAUSE|RESUME|RESET|COLOR|SPEED|KEY (?:3[2-9]|[4-9]\d|1[01]\d|12[0-6]))$/.test(command)) throw new Error('不支援的指令。');
    this.pending++;
    try { await this.writer.write(new TextEncoder().encode(command+'\n')); this.onLog('TX',command); }
    catch (e) { void this.disconnect(); throw e; }
    finally { this.pending--; }
  }
  async disconnect() {
    if (this.closing || !this.port) return;
    this.closing=true;
    try { await this.reader?.cancel(); } catch { /* USB device may already be gone */ }
    try { await this.readTask; } catch { /* already reported */ }
    try { await this.writer?.close(); } catch { /* unplug */ }
    try { this.writer?.releaseLock(); } catch { /* already released */ }
    this.writer=null;
    try { await this.port.close(); } catch { /* unplug */ }
    this.port=null; this.readTask=null; this.closing=false;
    this.onClose();
  }
}
