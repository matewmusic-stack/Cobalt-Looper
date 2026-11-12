#include "cobalt_looper.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define SR 48000
#define BL 64
static int fails=0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)
static lp_t P; static float *buf[2][2];
static float inL[BL],inR[BL],oL[BL],oR[BL],u1L[BL],u1R[BL];
static int64_t absn=0;
static void cmd(int type,int a,float f){ lp_cmd_t c={type,a,f,0}; lp_cmd(&P,c); }
/* input = absolute sample index, so playback can be checked exactly; track 1 left unarmed as a bypass check */
static void block(float *out0){
  for(int i=0;i<BL;i++){ inL[i]=oL[i]=(float)(absn+i); inR[i]=oR[i]=-(float)(absn+i); u1L[i]=.25f; u1R[i]=.25f; }
  float *io[2][2]={{oL,oR},{u1L,u1R}}; lp_process(&P,io,2,BL);
  for(int i=0;i<BL;i++) CHECK(u1L[i]==.25f,"unarmed track altered");
  if(out0) for(int i=0;i<BL;i++) out0[i]=oL[i];
  absn+=BL;
}
static void run(int64_t frames){ for(int64_t i=0;i<frames;i+=BL) block(0); }
static void setup(void){
  lp_init(&P,SR); absn=0;
  for(int t=0;t<2;t++) for(int c=0;c<2;c++){ buf[t][c]=calloc(SR*20,4); }
  lp_attach(&P,0,buf[0][0],buf[0][1],SR*20); lp_attach(&P,1,buf[1][0],buf[1][1],SR*20);
}
static void teardown(void){ for(int t=0;t<2;t++)for(int c=0;c<2;c++) free(buf[t][c]); }

static void test_internal(double bpm){
  setup(); cmd(LP_CMD_BPM,0,(float)bpm); cmd(LP_CMD_WET,0,1.f); cmd(LP_CMD_ARM_TOGGLE,0,0);
  run(BL); double bl=SR*60.0*4/bpm;
  run(30000); cmd(LP_CMD_TAP,0,0);                 /* tap mid-bar */
  block(0); CHECK(lp_light(&P)==2,"light should blink while waiting");
  run((int64_t)bl*2); CHECK(lp_light(&P)==1,"light should be solid while recording");
  /* find where recording started: block-aligned check via loop length later */
  cmd(LP_CMD_TAP,0,0); run((int64_t)bl*2);          /* stop request lands on a bar line */
  CHECK(lp_light(&P)==0,"light should be off after stop");
  size_t L=P.loop_len; double bars=L/bl;
  CHECK(fabs(bars-round(bars))<1e-4 && bars>=1,"loop is %.4f bars (bpm %.1f)",bars,bpm);
  /* loop playback must equal the audio recorded: out(T1+j) == in(T0+j) */
  float out[BL]; int64_t t0=-1;
  for(int i=0;i<(int)(L/BL)+4;i++){ block(out); }
  /* recorded content of track 0 left buffer is the abs index of each frame: check monotonic ramp */
  int ok=1; for(size_t j=1;j<L;j++) if(buf[0][0][j]!=buf[0][0][j-1]+1.f){ ok=0; break; }
  CHECK(ok,"recorded take is not contiguous"); (void)t0;
  double first=buf[0][0][0]; CHECK(fmod(first,bl)<1.5||bl-fmod(first,bl)<1.5,"take started off the bar line (%.1f)",fmod(first,bl));
  /* during playback, wet=1 output must be the ramp wrapped inside the loop */
  float prev=-1; int wraps=0,bad=0;
  for(int i=0;i<(int)(L*2/BL);i++){ block(out); for(int k=0;k<BL;k++){ if(prev>=0){ if(out[k]==prev+1) ; else if(out[k]==(float)first) wraps++; else bad++; } prev=out[k]; } }
  CHECK(bad==0 && wraps>=1,"playback glitch: bad=%d wraps=%d",bad,wraps);
  /* overdub then hold-to-clear */
  cmd(LP_CMD_TAP,0,0); run((int64_t)bl*2); CHECK(lp_light(&P)==1,"overdub should be recording");
  cmd(LP_CMD_TAP,0,0); run((int64_t)bl*2); CHECK(lp_light(&P)==0 && P.playing,"overdub should end and keep playing");
  cmd(LP_CMD_HOLD,0,0); block(0); CHECK(!P.playing&&P.loop_len==0,"hold should clear");
  teardown();
}
static void test_external(void){
  setup(); cmd(LP_CMD_SRC,1,0); cmd(LP_CMD_WET,0,1.f); cmd(LP_CMD_ARM_TOGGLE,0,0);
  const double tick=SR*60.0/(120*24);                      /* ideal 120 BPM tick spacing */
  int64_t nexti=0; int ticks=0; cmd(LP_CMD_START,0,0);
  for(int64_t f=0; f<SR*8; f+=BL){
    while((int64_t)(nexti*tick)<f+BL){ lp_cmd_t c={LP_CMD_TICK,0,0,(int64_t)(nexti*tick)}; lp_cmd(&P,c); nexti++; ticks++; }
    block(0);
    if(f==SR*3) cmd(LP_CMD_TAP,0,0);
    if(f==SR*3+SR*4) cmd(LP_CMD_TAP,0,0);
  }
  CHECK(fabs(P.bpm-120)<0.3,"bpm estimate %.3f",P.bpm);
  CHECK(P.loop_len>0 && fabs(fmod(P.loop_len,P.bar_len))<2.5,"ext loop %zu vs bar %.1f",P.loop_len,P.bar_len);
  /* stop: audio untouched and clock frozen; clock loss: same */
  int64_t c0=P.clk; cmd(LP_CMD_STOP,0,0); run(BL*8);
  CHECK(P.clk==c0,"clock should freeze on STOP");
  float o[BL]; block(o); CHECK(o[5]==(float)(absn-BL+5),"audio should pass through while stopped");
  cmd(LP_CMD_CONT,0,0); run(BL); CHECK(P.clk>c0,"clock should resume on CONTINUE");
  run(SR); CHECK(!P.run,"clock loss should pause the transport");
  teardown();
}
int main(void){
  test_internal(120); test_internal(97.3); test_internal(60);
  test_external();
  printf(fails?"%d FAILED\n":"all passed\n",fails); return fails!=0;
}
