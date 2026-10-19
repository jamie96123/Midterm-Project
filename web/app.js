import {PracticeGame} from './game.js';
import {SerialLink} from './serial.js';
const $=id=>document.getElementById(id);
const now=()=>Math.floor(performance.now()/10)*10;
const practice=new PracticeGame(now());
let mode='practice', snapshot=practice, connected=false, online=false, lastPacket=0, busy=false;
let previous={state:'ready',correct:0,wrong:0,misses:0,word:'',pos:-1}, noticeTimer;
let logLines=[], logNumber=0;
const rateNames=['舒適 · 8 秒','專注 · 5 秒','極速 · 3 秒'];
const stateNames={ready:'準備好，進入心流',running:'挑戰進行中，保持你的節奏',paused:'暫停中，深呼吸再繼續',finished:'挑戰完成，做得好！'};
function notify(message) { $('notice').textContent=message; $('notice').hidden=false; clearTimeout(noticeTimer); noticeTimer=setTimeout(()=>$('notice').hidden=true,4500); }
function log(direction,message) {
  const stamp=new Date().toLocaleTimeString('zh-TW',{hour12:false});
  logLines.push(`[${stamp}] ${direction}  ${message}`);
  if (logLines.length>80) logLines.shift();
  $('serialLog').textContent=logLines.join('\n'); $('serialLog').scrollTop=$('serialLog').scrollHeight;
  $('logCount').textContent=`${++logNumber} 則`;
  if (direction==='ERROR') notify(message);
}
const link=new SerialLink(g=>{
  if (mode!=='hardware') return;
  const wasRunning=snapshot.state==='running';
  snapshot=g; lastPacket=performance.now();
  if (!online) { online=true; $('connectionInfo').textContent='已連接 STM32 · TypeRush v1 · 狀態同步正常'; }
  render();
  if (!wasRunning && g.state==='running' && !document.querySelector('dialog[open]')) $('typingInput').focus();
},log,()=>{
  connected=false; online=false;
  $('connectionInfo').textContent='連線已中斷。請重新連接，或切換練習模式。';
  log('INFO','連線已中斷'); render();
});
function playable() { return mode==='practice' || (connected && online); }
async function command(line) {
  if (!playable()) { notify('尚未收到開發板狀態，請先連線或使用練習模式。'); return; }
  if (mode==='practice') { practice.command(line,now()); snapshot=practice; render(); }
  else {
    try { await link.send(line); }
    catch(e) { notify(`傳送失敗：${e.message}`); }
  }
}
function feedback(text,type='') { $('feedback').textContent=text; $('feedback').className=`feedback ${type}`; }
function render() {
  const g=snapshot, available=playable(), running=g.state==='running' && available;
  const hardware=mode==='hardware';
  $('connectionBadge').className=`connection-badge${hardware && online?' connected':''}`;
  // Keep the indicator element; all dynamic content is inserted as text.
  $('connectionBadge').replaceChildren(Object.assign(document.createElement('i'),{}),document.createTextNode(hardware?(online?'STM32 已連線':connected?'等待板端回應':'STM32 未連線'):'練習模式'));
  $('modeLabel').textContent=hardware?'STM32 挑戰':'自由練習';
  $('modeNote').textContent=hardware?'板端計時與計分':'無需連接開發板';
  $('stateLabel').textContent=!available?(connected?'等待 STM32 回應，請確認連線參數':'開發板未連接，或切換練習模式'):stateNames[g.state];
  $('round').textContent=String(Math.max(1,g.question)).padStart(2,'0');
  $('rateLabel').textContent=rateNames[g.rate];
  $('time').replaceChildren(document.createTextNode((g.remaining/1000).toFixed(2)),Object.assign(document.createElement('span'),{textContent:'s'}));
  $('timeBar').style.width=`${g.remaining/g.duration*100}%`;
  $('arenaBody').dataset.color=g.color;
  const word=g.word || 'ready';
  if (previous.word!==word || previous.pos!==g.pos || previous.state!==g.state) {
    const nodes=[...word].map((c,index)=>{
      const span=document.createElement('span'); span.textContent=c;
      span.className=g.word && index<g.pos?'done':g.word && index===g.pos && g.state!=='finished'?'next':'';
      return span;
    });
    $('targetWord').replaceChildren(...nodes); $('targetWord').setAttribute('aria-label',`目標單字：${word}`);
  }
  $('wordCaption').textContent=g.state==='ready'?'按下開始，點亮你的第一個單字':g.state==='finished'?'這一場，你的節奏已留下紀錄':g.state==='paused'?'倒數已凍結，燈號維持恆亮':'依序輸入下方單字';
  $('score').textContent=g.score;
  const wpm=g.elapsed>0?Math.round(g.hits*12000/g.elapsed):0;
  const accuracy=g.attempts?Math.round(g.hits/g.attempts*100):null;
  $('wpm').replaceChildren(document.createTextNode(wpm),Object.assign(document.createElement('small'),{textContent:' WPM'}));
  $('accuracy').replaceChildren(document.createTextNode(accuracy??'—'),Object.assign(document.createElement('small'),{textContent:' %'}));
  $('correct').replaceChildren(document.createTextNode(g.correct),Object.assign(document.createElement('small'),{textContent:' / 10'}));
  $('errors').replaceChildren(document.createTextNode(g.wrong),Object.assign(document.createElement('small'),{textContent:` / ${g.misses}`}));
  $('typingInput').disabled=!running;
  $('typingInput').placeholder=g.state==='paused'?'暫停中，繼續後再輸入…':'點一下這裡，輸入目標單字…';
  $('startButton').disabled=!available || g.state==='running';
  $('startButton').textContent=g.state==='paused'?'▶ 繼續挑戰':g.state==='finished'?'↗ 再次挑戰':'▶ 開始挑戰';
  $('pauseButton').disabled=!available || !['running','paused'].includes(g.state);
  $('pauseButton').textContent=g.state==='paused'?'▶ 繼續':'Ⅱ 暫停';
  for (const id of ['colorButton','speedButton','resetButton']) $(id).disabled=!available;
  $('connectButton').disabled=busy || connected;
  $('disconnectButton').disabled=!connected;
  for (const id of ['baudRate','dataBits','stopBits','parity','flowControl']) $(id).disabled=connected || busy;
  $('syncButton').disabled=!connected;
  const colorNames={green:'綠光 · 極速反應',yellow:'黃光 · 穩定過關',red:'紅光 · 驚險壓線',blue:'待機藍光'};
  $('lightName').textContent=colorNames[g.color];
  $('lightDescription').textContent=g.state==='paused'?'暫停恆亮 · 時間已凍結':g.state==='running'?`${g.manual?'手動展示':'依剩餘時間變色'} · 呼吸律動`:'準備迎接下一次挑戰';
  $('led').className=`led ${g.color}${running?' breathing':''}`;
  $('led').style.setProperty('--breath',`${[1800,1200,700][g.rate]}ms`);
  $('lightHint').textContent=hardware?'畫面依板端狀態顯示；實體 LED 由 STM32 驅動。':'練習模式為畫面模擬；接板後由實體 LED 呈現。';
  if (g.state==='ready') feedback('10 個單字，一場屬於你的節奏挑戰。');
  else if (g.correct>previous.correct) feedback(`漂亮！完成單字 +${g.lastAward} 分。`,'success');
  else if (g.wrong>previous.wrong) {
    feedback('這個字母不對，再看一次游標的位置。','error');
    $('targetWord').classList.remove('shake'); void $('targetWord').offsetWidth; $('targetWord').classList.add('shake');
  } else if (g.misses>previous.misses) feedback('這題時間到，下一題重新找回節奏。','error');
  else if (previous.state==='ready' && g.state==='running') feedback('逐字輸入，最後一字完成就會自動換題。');
  else if (g.state==='paused') feedback('休息一下，按「繼續」回到挑戰。');
  else if (previous.state==='paused' && g.state==='running') feedback('回到你的節奏，繼續挑戰。');
  if (g.state==='finished' && previous.state!=='finished') {
    $('resultScore').textContent=g.score;
    $('resultMode').textContent=hardware?'STM32 板端成績':'練習模式成績 · 未連接開發板';
    $('resultDetails').textContent=`完成 ${g.correct} / 10 題 · 逾時 ${g.misses} 題\n${wpm} WPM · 正確率 ${accuracy??0}% · 錯字 ${g.wrong} 次`;
    $('resultDetails').style.whiteSpace='pre-line';
    $('resultDialog').showModal();
  }
  previous={state:g.state,correct:g.correct,wrong:g.wrong,misses:g.misses,word,pos:g.pos};
}
async function start() {
  if ($('resultDialog').open) $('resultDialog').close();
  await command(snapshot.state==='paused'?'RESUME':'START');
  $('typingInput').focus();
}
$('startButton').addEventListener('click',start);
$('playAgainButton').addEventListener('click',start);
$('pauseButton').addEventListener('click',async()=>{await command(snapshot.state==='paused'?'RESUME':'PAUSE'); if (snapshot.state==='running') $('typingInput').focus();});
$('resetButton').addEventListener('click',()=>command('RESET'));
$('colorButton').addEventListener('click',()=>command('COLOR'));
$('speedButton').addEventListener('click',()=>command('SPEED'));
$('typingInput').addEventListener('paste',e=>{e.preventDefault(); notify('請逐字輸入，這是一場手速挑戰。');});
$('typingInput').addEventListener('drop',e=>e.preventDefault());
$('typingInput').addEventListener('input',e=>{
  if (e.isComposing) return;
  const value=$('typingInput').value; $('typingInput').value='';
  if (!value) return;
  if (value.length!==1 || value.charCodeAt(0)<32 || value.charCodeAt(0)>126) { notify('請切換英文輸入法，逐字輸入英文。'); return; }
  if (snapshot.state==='running') void command(`KEY ${value.charCodeAt(0)}`);
});
for (const [button,dialog] of [['settingsButton','settingsDialog'],['helpButton','helpDialog'],['technicalButton','technicalDialog']]) {
  $(button).addEventListener('click',()=>$(dialog).showModal());
}
for (const button of document.querySelectorAll('.close-dialog')) button.addEventListener('click',()=>button.closest('dialog').close());
for (const dialog of document.querySelectorAll('dialog')) dialog.addEventListener('click',e=>{ if (e.target===dialog) { const r=dialog.getBoundingClientRect(); if(e.clientX<r.left || e.clientX>r.right || e.clientY<r.top || e.clientY>r.bottom) dialog.close(); } });
$('serialForm').addEventListener('submit',async e=>{
  e.preventDefault(); if (busy || connected) return;
  const settings={baudRate:Number($('baudRate').value),dataBits:Number($('dataBits').value),stopBits:Number($('stopBits').value),parity:$('parity').value,flowControl:$('flowControl').value};
  if (settings.baudRate!==115200 || settings.dataBits!==8 || settings.stopBits!==1 || settings.parity!=='none' || settings.flowControl!=='none') {
    $('connectionInfo').textContent='隨附韌體僅支援 115200 / 8N1 / 無流量控制，請還原參數，或先修改並燒錄相同參數的韌體。';
    // Custom settings are available for users who deliberately update their board firmware.
    log('INFO','使用自訂串列參數，請確認板端已同步修改。');
  }
  busy=true; render(); $('connectionInfo').textContent='請在瀏覽器視窗選擇 STLink Virtual COM Port…';
  try {
    await link.connect(settings);
    practice.command('PAUSE',now());
    mode='hardware'; connected=true; online=false;
    snapshot=new PracticeGame(now()); previous={state:'ready',correct:0,wrong:0,misses:0,word:'',pos:-1};
    lastPacket=performance.now();
    $('connectionInfo').textContent='序列埠已開啟，正在等待 TypeRush v1 回應…';
    log('INFO',`開啟 ${settings.baudRate} / ${settings.dataBits}${settings.parity==='none'?'N':settings.parity==='even'?'E':'O'}${settings.stopBits} / ${settings.flowControl}`);
    await link.send('SYNC');
  } catch(error) {
    $('connectionInfo').textContent=error.name==='NotFoundError'?'已取消選擇序列埠。':`無法連接：${error.message}`;
    if (error.name!=='NotFoundError') notify($('connectionInfo').textContent);
    if (link.port) await link.disconnect();
  } finally { busy=false; render(); }
});
$('disconnectButton').addEventListener('click',()=>link.disconnect());
$('practiceButton').addEventListener('click',async()=>{
  if (busy) return;
  if (connected && online && snapshot.state==='running') { try { await link.send('PAUSE'); } catch { /* cleanup below */ } }
  await link.disconnect();
  mode='practice'; practice.reset(now()); snapshot=practice;
  previous={state:'ready',correct:0,wrong:0,misses:0,word:'',pos:-1};
  $('settingsDialog').close(); render(); notify('已切換為練習模式。');
});
$('syncButton').addEventListener('click',async()=>{try {await link.send('SYNC');} catch(e){notify(e.message);}});
$('clearLogButton').addEventListener('click',()=>{logLines=[];logNumber=0;$('serialLog').textContent='';$('logCount').textContent='0 則';});
const checklist=[
  ['GPIO 輸出 / 複用功能','PA5 狀態燈；PA6 / PA7 / PB0 三色燈；PA2 / PA3 USART2 AF7。','board.c · gpio_init / alternate / board_lights'],
  ['GPIO 輸入 / EXTI','PB4–PB7 上拉輸入、下降緣中斷，連接四顆接地按鈕。','board.c · gpio_init / button_irq'],
  ['PLL 170 MHz','HSI16 ÷4 ×85 ÷2；Range 1 boost、FLASH 4 等待週期。','board.c · clock_init'],
  ['NVIC / 中斷優先權','TIM2 優先 0、USART2 優先 1、EXTI 優先 2；主迴圈處理事件。','board.c · timers_init / uart_init / gpio_init'],
  ['軟體去彈跳','四個按鈕各自以時間戳限制 50 ms 內重複觸發。','board.c · button_irq'],
  ['UART 傳送與環形接收','115200 8N1；RXNE IRQ → 512-byte RX ring；TXE IRQ → TX ring。','board.c · USART2_IRQHandler / board_send'],
  ['Timer 定時中斷','TIM2 PSC=169、ARR=9999，每 10 ms 更新倒數基準。','board.c · TIM2_IRQHandler'],
  ['PWM 呼吸燈','TIM3 三通道 1 kHz；暫停以 MODER / BSRR 改為 GPIO 恆亮。','board.c · timers_init / board_lights'],
];
for (const [title,description,source] of checklist) {
  const row=document.createElement('div');row.className='check-row';
  const mark=document.createElement('span');mark.textContent='☑';
  const body=document.createElement('div'), strong=document.createElement('strong'), p=document.createElement('p'), code=document.createElement('code');
  strong.textContent=title;p.textContent=description;code.textContent=source;body.append(strong,p,code);row.append(mark,body);$('checklist').append(row);
}
const unused=document.createElement('div');unused.className='check-row unused';unused.textContent='☐ 輸入捕捉：本遊戲未使用。';$('checklist').append(unused);
setInterval(()=>{
  if (mode==='practice') { practice.update(now()); snapshot=practice; render(); }
  else if (connected && performance.now()-lastPacket>1500) {
    if (online) notify('超過 1.5 秒未收到板端回應，已停止鍵盤傳送。');
    online=false; $('connectionInfo').textContent='未收到有效狀態。請確認韌體、鮑率、USB 資料線，或按「同步狀態」。'; render();
  }
},30);
document.addEventListener('visibilitychange',()=>{
  if (document.hidden && snapshot.state==='running' && playable()) {
    void command('PAUSE'); notify('切換分頁時已送出暫停，回來後可繼續。');
  }
});
render();
