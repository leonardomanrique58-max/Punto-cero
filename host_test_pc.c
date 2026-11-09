#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../punto-cero-ps2/src/platform.h"
int SW = 640, SH = 448;
static unsigned char *fb; static int *zb;
void gfx_init(void){ fb = malloc(SW*SH*3); zb = malloc(SW*SH*sizeof(int)); }
void gfx_begin(u32c c){ for(int i=0;i<SW*SH;i++){fb[i*3]=CR(c);fb[i*3+1]=CG(c);fb[i*3+2]=CB(c);zb[i]=0;} }
void gfx_end(void){}
void gfx_tri(float x1,float y1,int z1,u32c c1,float x2,float y2,int z2,u32c c2,float x3,float y3,int z3,u32c c3){
  float minx=fminf(x1,fminf(x2,x3)),maxx=fmaxf(x1,fmaxf(x2,x3)),miny=fminf(y1,fminf(y2,y3)),maxy=fmaxf(y1,fmaxf(y2,y3));
  int X0=fmaxf(0,floorf(minx)),X1=fminf(SW-1,ceilf(maxx)),Y0=fmaxf(0,floorf(miny)),Y1=fminf(SH-1,ceilf(maxy));
  float d=(y2-y3)*(x1-x3)+(x3-x2)*(y1-y3); if(fabsf(d)<1e-6f) return;
  for(int y=Y0;y<=Y1;y++)for(int x=X0;x<=X1;x++){
    float px=x+.5f,py=y+.5f;
    float a=((y2-y3)*(px-x3)+(x3-x2)*(py-y3))/d,b=((y3-y1)*(px-x3)+(x1-x3)*(py-y3))/d,c=1-a-b;
    if(a<0||b<0||c<0) continue;
    int z=(int)(a*z1+b*z2+c*z3); if(z<zb[y*SW+x]) continue; zb[y*SW+x]=z;
    unsigned char *p=fb+(y*SW+x)*3;
    p[0]=a*CR(c1)+b*CR(c2)+c*CR(c3); p[1]=a*CG(c1)+b*CG(c2)+c*CG(c3); p[2]=a*CB(c1)+b*CB(c2)+c*CB(c3);
  }
}
void gfx_rect(float x1,float y1,float x2,float y2,u32c c){
  int X0=fmaxf(0,roundf(x1)),X1=fminf(SW,roundf(x2)),Y0=fmaxf(0,roundf(y1)),Y1=fminf(SH,roundf(y2)); float a=CA(c)/255.f;
  for(int y=Y0;y<Y1;y++)for(int x=X0;x<X1;x++){unsigned char*p=fb+(y*SW+x)*3;p[0]=p[0]*(1-a)+CR(c)*a;p[1]=p[1]*(1-a)+CG(c)*a;p[2]=p[2]*(1-a)+CB(c)*a;}
}
void pad_init(void){} void pad_poll(Pad*p){memset(p,0,sizeof(*p));}
static void shot(const char*n){FILE*f=fopen(n,"wb");fprintf(f,"P6\n%d %d\n255\n",SW,SH);fwrite(fb,1,SW*SH*3,f);fclose(f);}
int main(int argc,char**argv){
  gfx_init(); game_init(); Pad p; memset(&p,0,sizeof p); float dt=1/60.f;
  for(int i=0;i<30;i++) game_frame(&p,dt); shot("menu.ppm");
  p.pressed=B_DOWN; game_frame(&p,dt); p.pressed=B_CROSS; game_frame(&p,dt); p.pressed=0; for(int i=0;i<5;i++) game_frame(&p,dt); shot("settings.ppm");
  p.pressed=B_CIRCLE; game_frame(&p,dt); p.pressed=B_UP; game_frame(&p,dt); p.pressed=B_CROSS; game_frame(&p,dt); p.pressed=0;
  for(int i=0;i<200;i++) game_frame(&p,dt); shot("game1.ppm");
  /* mover y girar, disparar */
  p.ly=-1; p.rx=0.5f; p.down=B_R2; for(int i=0;i<240;i++){ p.pressed=0; game_frame(&p,dt);} shot("game2.ppm");
  p.rx=-0.3f; p.ly=-0.5f; for(int i=0;i<600;i++){ game_frame(&p,dt);} shot("game3.ppm");
  return 0; }
