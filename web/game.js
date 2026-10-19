// Practice mode mirrors firmware/src/game.c. Hardware mode never computes game scores here.
export const WORDS = ['rhythm','light','timer','signal','board','focus','pulse','typing','clock','green','yellow','speed','serial','buffer','button','score','bright','flow','micro','pixel','logic','react','power','flash','input','output','energy','swift','challenge','breathe','spark','wave','orbit','shift','dream','coding','tempo','motion','switch','level'];
export const DURATIONS = [8000, 5000, 3000];
export function tier(g) {
  return g.remaining * 100 > g.duration * 60 ? 'green' : g.remaining * 100 >= g.duration * 30 ? 'yellow' : 'red';
}
export class PracticeGame {
  constructor(now = 0) { this.reset(now); }
  reset(now) {
    Object.assign(this, {t:'state', v:1, state:'ready', word:'', pos:0, score:0, correct:0,
      wrong:0, misses:0, attempts:0, hits:0, question:0, total:10, remaining:8000, duration:8000,
      elapsed:0, rate:0, color:'blue', manual:0, lastAward:0, result:'ready', rxErrors:0,
      txDrops:0, protocolErrors:0, lastTick:now, random:0x474});
  }
  next() {
    if (++this.question > this.total) {
      this.question = this.total; this.state = 'finished'; this.remaining = 0; return;
    }
    this.random = (Math.imul(this.random, 1664525) + 1013904223) >>> 0;
    this.word = WORDS[(this.random >>> 16) % WORDS.length];
    this.duration = DURATIONS[this.rate]; this.remaining = this.duration; this.pos = 0; this.manual = 0;
  }
  update(now) {
    let delta = Math.max(0, now - this.lastTick); this.lastTick = now;
    while (this.state === 'running' && delta > 0) {
      const spent = Math.min(delta, this.remaining);
      this.remaining -= spent; this.elapsed += spent; delta -= spent;
      if (this.remaining <= 0) { this.misses++; this.lastAward=0; this.result='timeout'; this.next(); }
    }
    if (!this.manual && ['running','paused'].includes(this.state)) this.color = tier(this);
    return this;
  }
  command(line, now) {
    this.update(now);
    switch (line) {
      case 'SYNC': break;
      case 'RESET': this.reset(now); break;
      case 'START': {
        const rate = this.rate, seed = this.random ^ (now >>> 0);
        this.reset(now); this.rate = rate; this.random = seed >>> 0;
        this.state='running'; this.result='started'; this.next(); break;
      }
      case 'PAUSE': if (this.state==='running') { this.state='paused'; this.result='paused'; } break;
      case 'RESUME': if (this.state==='paused') { this.state='running'; this.result='resumed'; } break;
      case 'COLOR': this.color=['green','yellow','red'][(['green','yellow','red'].indexOf(this.color)+1)%3]; this.manual=1; this.result='color'; break;
      case 'SPEED': {
        this.rate=(this.rate+1)%3;
        const duration=DURATIONS[this.rate];
        this.remaining=Math.ceil(this.remaining*duration/this.duration); this.duration=duration; this.result='speed'; break;
      }
      default: {
        if (!/^KEY (?:3[2-9]|[4-9]\d|1[01]\d|12[0-6])$/.test(line)) return false;
        if (this.state!=='running') break;
        const c=String.fromCharCode(Number(line.slice(4)));
        this.attempts++;
        if (c===this.word[this.pos]) {
          this.hits++; this.pos++; this.result='hit';
          if (this.pos===this.word.length) {
            this.lastAward={green:7,yellow:5,red:3}[tier(this)];
            this.score+=this.lastAward; this.correct++; this.result='correct'; this.next();
          }
        } else { this.wrong++; this.result='wrong'; }
      }
    }
    this.update(now); return true;
  }
}
