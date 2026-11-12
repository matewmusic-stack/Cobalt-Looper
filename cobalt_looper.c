#include "cobalt_looper.h"
#include <string.h>
enum { M_NEW, M_DUB };
static inline float clampf(float x){ return x>1.f?1.f:(x<-1.f?-1.f:x); }

static void recompute(lp_t *p){
  p->bar_len = (double)p->sr*60.0*LP_BEATS_PER_BAR/p->bpm;
  double m = p->bar_len*LP_MAX_BARS;
  p->max_len = (m < (double)p->cap) ? (size_t)m : p->cap;
}
void lp_init(lp_t *p, float sr){
  memset(p,0,sizeof *p); p->sr=sr; p->bpm=120; p->wet=.7f; p->start_at=p->stop_at=-1; p->run=1; recompute(p);
}
void lp_attach(lp_t *p, int t, float *l, float *r, size_t n){
  if(t<0||t>=LP_MAX_TRACKS) return;
  p->tr[t].buf[0]=l; p->tr[t].buf[1]=r; p->tr[t].len=n;
  size_t cap=0; int first=1;
  for(int i=0;i<LP_MAX_TRACKS;i++) if(p->tr[i].len){ if(first||p->tr[i].len<cap) cap=p->tr[i].len; first=0; }
  p->cap=cap; recompute(p);
}
int lp_cmd(lp_t *p, lp_cmd_t c){
  uint32_t nx=(p->qh+1)&(LP_QN-1); if(nx==p->qt) return 0;
  p->q[p->qh]=c; LP_BARRIER(); p->qh=nx; return 1;
}
int64_t lp_now(const lp_t *p){ return p->abs; }
int lp_light(const lp_t *p){ return p->recording?1:(p->start_at>=0?2:0); }
int lp_bars(const lp_t *p){ return p->loop_len? (int)((double)p->loop_len/p->bar_len+.5):0; }

static int64_t next_bar(const lp_t *p){   /* first bar line after "now", plus the lead */
  double b=(double)((int64_t)((double)p->clk/p->bar_len))+1.0+p->lead;
  return (int64_t)(b*p->bar_len+.5);
}
static void end_rec(lp_t *p){
  p->recording=0;
  if(p->mode==M_NEW){
    p->loop_len = p->wp<p->max_len?p->wp:p->max_len;
    if(p->loop_len){ p->playing=1; p->pos=0; } else for(int i=0;i<LP_MAX_TRACKS;i++) p->tr[i].has=0;
  }
}
static void clear_all(lp_t *p){   /* no memset: 'has' gates every read, new takes overwrite */
  p->start_at=p->stop_at=-1; p->recording=0; p->playing=0; p->loop_len=0; p->pos=0; p->wp=0;
  for(int i=0;i<LP_MAX_TRACKS;i++) p->tr[i].has=0;
}
static void tap(lp_t *p){
  if(p->start_at>=0){ p->start_at=-1; return; }
  if(p->recording){ p->stop_at = (p->stop_at>=0)?-1:next_bar(p); return; }
  int any=0; for(int i=0;i<LP_MAX_TRACKS;i++) any|=p->tr[i].armed;
  if(any) p->start_at=next_bar(p);
}
static void tick_in(lp_t *p, int64_t t){
  if(!p->ext) return;
  p->last_tick=p->abs;
  if(!p->clk_ok){ p->clk_ok=1; p->n=0; p->E=0; p->clk=0; p->has_last=0; p->start_at=p->stop_at=-1; if(p->playing) p->pos=0; }
  if(p->has_last && t>p->last_t){
    double b=60.0*p->sr/((double)(t-p->last_t)*LP_PPQN);
    if(b<30) b=30;
    if(b>300) b=300;
    if(p->n<2) p->bpm=b; else if(b>p->bpm*.5 && b<p->bpm*2) p->bpm+=(b-p->bpm)*(p->n<48?.25:.04);
    recompute(p);
  }
  p->last_t=t; p->has_last=1;
  if(p->stopped) return;
  if(p->n++>0){ p->E+=p->bar_len/(LP_BEATS_PER_BAR*LP_PPQN); p->clk+=(int64_t)((p->E-(double)p->clk)*.1); }
}
static void handle(lp_t *p, const lp_cmd_t *c){
  switch(c->type){
  case LP_CMD_ARM_TOGGLE: if(c->a<LP_MAX_TRACKS && p->tr[c->a].len) p->tr[c->a].armed^=1; break;
  case LP_CMD_TAP: tap(p); break;
  case LP_CMD_HOLD: clear_all(p); break;
  case LP_CMD_WET: p->wet=c->f<0?0:(c->f>1?1:c->f); break;
  case LP_CMD_BPM: if(!p->ext && !p->loop_len && !p->recording && p->start_at<0 && c->f>=30 && c->f<=300){ p->bpm=c->f; recompute(p); p->clk=0; } break;
  case LP_CMD_LEAD: p->lead=c->a?1:0; break;
  case LP_CMD_SRC: p->ext=c->a?1:0; p->clk_ok=0; p->stopped=0; p->has_last=0; p->n=0; break;
  case LP_CMD_TICK: tick_in(p,c->t); break;
  case LP_CMD_START: p->n=0; p->E=0; p->clk=0; p->has_last=0; p->stopped=0; p->clk_ok=1; p->last_tick=p->abs;
                     p->start_at=p->stop_at=-1; if(p->playing) p->pos=0; break;
  case LP_CMD_STOP: p->stopped=1; break;
  case LP_CMD_CONT: p->stopped=0; break;
  }
}
void lp_process(lp_t *p, float *const io[][2], int ntr, int frames){
  while(p->qt!=p->qh){ lp_cmd_t c=p->q[p->qt]; LP_BARRIER(); p->qt=(p->qt+1)&(LP_QN-1); handle(p,&c); }
  if(p->ext){ if(p->abs-p->last_tick>(int64_t)(.6*p->sr)) p->clk_ok=0; p->run=p->clk_ok&&!p->stopped; } else p->run=1;
  if(!p->run){ p->abs+=frames; return; }          /* external clock stopped/lost: audio passes untouched */
  if(ntr>LP_MAX_TRACKS) ntr=LP_MAX_TRACKS;
  const float w=p->wet, dw=1.f-w;
  for(int k=0;k<frames;k++){
    if(p->start_at>=0 && p->clk>=p->start_at){ p->start_at=-1; p->recording=1; p->mode=p->loop_len?M_DUB:M_NEW; p->wp=0; }
    if(p->stop_at>=0 && p->clk>=p->stop_at){ p->stop_at=-1; end_rec(p); }
    for(int t=0;t<ntr;t++){
      lp_track_t *T=&p->tr[t]; if(!T->armed||!T->len) continue;
      float xl=io[t][0][k], xr=io[t][1][k];
      if(p->recording && p->mode==M_NEW){ T->buf[0][p->wp]=xl; T->buf[1][p->wp]=xr; T->has=1; }
      if(p->playing && T->has){
        float ll=T->buf[0][p->pos], lr=T->buf[1][p->pos];
        if(p->recording && p->mode==M_DUB){ T->buf[0][p->pos]=clampf(ll+xl); T->buf[1][p->pos]=clampf(lr+xr); }
        io[t][0][k]=xl*dw+ll*w; io[t][1][k]=xr*dw+lr*w;      /* Cue out replaces the channel */
      } else if(p->playing && p->recording && p->mode==M_DUB){ T->has=1; T->buf[0][p->pos]=xl; T->buf[1][p->pos]=xr; }
    }
    if(p->playing && ++p->pos>=p->loop_len) p->pos=0;
    if(p->recording && p->mode==M_NEW && ++p->wp>=p->max_len){ p->stop_at=-1; end_rec(p); }
    p->clk++;
  }
  p->abs+=frames;
}
