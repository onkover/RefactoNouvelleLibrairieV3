// v3sim.cpp — simulateur du pipeline de rendu de LibraryV3 pour la scene V3 (sondes hors moteur).
// Reproduit : projection perspective (fov vertical 60, near 0.1, far infini, depth inversee),
// clipping near (Sutherland-Hodgman), face avant = aire raster < 0, boite serree, echange p1<->p2,
// regle top-left, centre de pixel +0.5, test de profondeur 1/w strict. Calcul en double : ce sont des
// PREDICTIONS de compteurs (ordre de grandeur et structure), pas une reference bit a bit.
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <array>
using namespace std;
struct V3{double x,y,z;};
struct Cl{double x,y,w; };          // clip : x,y,w (z inutile pour les tests)
struct Cam{ char name[64]; V3 pos; double yaw; };
struct Stat{
  long submitted=0, clipped=0, back=0, front=0, earlyRej=0, drawn=0;
  long cov0=0,cov1=0,cov24=0,cov5=0, pix1=0, simd=0;
  long tight=0, covered=0, depthPassed=0;
  long cls[8]={0}; long clsTight[8]={0}, clsCov[8]={0};
  long blocks=0, blocksEmpty=0, blocksFull=0, lanesCovered=0;
  long scalarTight=0;
  long tiles=0, tReject=0, tAccept=0, tPartial=0, tileRowsEdge=0, tileRowsFill=0;
  long holes=0, vpPixels=0;
  long tt[5]={0}, trj[5]={0}, tac[5]={0}, tpa[5]={0}, tEdgeBlk[5]={0}, tFillBlk[5]={0};
  long fxCov8=0, fxDiff8=0, fxCov4=0, fxDiff4=0, fxCovFloat=0;
  long thin=0; long narrow=0, simdTight=0;  // front, drawn, box area>=64 et Covered/Tight < 0.25
};
static double E(double ax,double ay,double bx,double by,double px,double py){ return (ax-px)*(by-py)-(ay-py)*(bx-px); }
static bool TL(double ax,double ay,double bx,double by){ double ex=bx-ax, ey=by-ay; return (ey==0&&ex<0)||(ey>0); }
int main(int argc,char**argv){
  vector<long> roomEnd; if(argc>5){ char* t=strtok(argv[5],","); while(t){ roomEnd.push_back(atol(t)); t=strtok(nullptr,","); } }
  const char* obj=argv[1]; const char* camf=argv[2]; int VW=argc>3?atoi(argv[3]):768, VH=argc>4?atoi(argv[4]):400;
  vector<V3> v; vector<array<int,3>> tri;
  FILE*f=fopen(obj,"r"); char line[256];
  while(fgets(line,256,f)){
    if(line[0]=='v'&&line[1]==' '){ V3 p; sscanf(line+2,"%lf %lf %lf",&p.x,&p.y,&p.z); v.push_back(p);}
    else if(line[0]=='f'){ int a,b,c,t1,t2,t3; sscanf(line+2,"%d/%d %d/%d %d/%d",&a,&t1,&b,&t2,&c,&t3); tri.push_back({a-1,b-1,c-1}); }
  }
  fclose(f);
  vector<Cam> cams; f=fopen(camf,"r");
  while(fgets(line,256,f)){ Cam c; sscanf(line,"%63s %lf %lf %lf %lf",c.name,&c.pos.x,&c.pos.y,&c.pos.z,&c.yaw); cams.push_back(c);} fclose(f);
  const double fov=60.0*M_PI/180, foc=1.0/tan(fov/2), asp=double(VW)/VH, nearP=0.1;
  printf("triangles=%zu positions=%zu viewport=%dx%d\n",tri.size(),v.size(),VW,VH);
  for(const Cam&cam:cams){
    Stat S; long roomBack[8]={0}; long roomDrawn[8]={0}, roomCovered[8]={0}, roomPassed[8]={0}; vector<unsigned char> owner(VW*VH,255); vector<long> idb(VW*VH,-1); vector<float> zb(VW*VH,0.f); vector<unsigned char> seen(VW*VH,0);
    double cy=cos(cam.yaw*M_PI/180), sy=sin(cam.yaw*M_PI/180);
    vector<Cl> cv(v.size());
    for(size_t i=0;i<v.size();++i){
      double dx=v[i].x-cam.pos.x, dy=v[i].y-cam.pos.y, dz=v[i].z-cam.pos.z;
      // monde -> camera : rotation d'angle -yaw autour de Y ; la camera regarde -Z
      double xc= cy*dx - sy*dz, zc= sy*dx + cy*dz;
      cv[i]={ foc/asp*xc, foc*dy, -zc };
    }
    long faceIdx=-1; long gTri=0; vector<long> tB(1,0), tC(1,0), tS(1,0);
    for(const auto& t:tri){
      S.submitted++; ++faceIdx; int rm=0; while(rm<(int)roomEnd.size()-1 && faceIdx>=roomEnd[rm]) rm++;
      Cl poly[6]; int n=0; Cl s[3]={cv[t[0]],cv[t[1]],cv[t[2]]};
      double d[3]={s[0].w-nearP,s[1].w-nearP,s[2].w-nearP};
      if(d[0]<0&&d[1]<0&&d[2]<0) continue;
      bool allin=(d[0]>=0&&d[1]>=0&&d[2]>=0);
      if(allin){ poly[0]=s[0];poly[1]=s[1];poly[2]=s[2]; n=3; }
      else { S.clipped++;
        for(int i=0;i<3;++i){ int j=(i+1)%3; if(d[i]>=0) poly[n++]=s[i];
          if((d[i]>=0)!=(d[j]>=0)){ double tt=d[i]/(d[i]-d[j]); Cl a=s[i],b=s[j]; poly[n++]={a.x+(b.x-a.x)*tt,a.y+(b.y-a.y)*tt,a.w+(b.w-a.w)*tt}; } } }
      for(int k=1;k+1<n;++k){
        Cl q[3]={poly[0],poly[k],poly[k+1]};
        double X[3],Y[3],IW[3];
        for(int i=0;i<3;++i){ double iw=1.0/q[i].w; X[i]=(q[i].x*iw*0.5+0.5)*VW; Y[i]=(0.5-q[i].y*iw*0.5)*VH; IW[i]=iw; }
        double area=(X[0]-X[2])*(Y[1]-Y[2])-(Y[0]-Y[2])*(X[1]-X[2]);
        if(!(area<0)){ S.back++; roomBack[rm]++; continue; }
        S.front++;
        double mnx=min({X[0],X[1],X[2]}),mxx=max({X[0],X[1],X[2]}),mny=min({Y[0],Y[1],Y[2]}),mxy=max({Y[0],Y[1],Y[2]});
        int x0=(int)ceil(mnx-0.5), y0=(int)ceil(mny-0.5), x1=(int)floor(mxx-0.5)+1, y1=(int)floor(mxy-0.5)+1;
        x0=max(x0,0);y0=max(y0,0);x1=min(x1,VW);y1=min(y1,VH);
        if(x0>=x1||y0>=y1){ S.earlyRej++; continue; }
        S.drawn++; gTri=S.drawn; tB.push_back(0); tC.push_back(0); tS.push_back(0);
        int bw=x1-x0, bh=y1-y0; long boxA=(long)bw*bh; S.tight+=boxA;
        // echange p1<->p2 comme SetupTriangle : p0 = a, p1 = c, p2 = b (aire > 0)
        double p0x=X[0],p0y=Y[0],p1x=X[2],p1y=Y[2],p2x=X[1],p2y=Y[1]; double iw0=IW[0],iw1=IW[2],iw2=IW[1];
        bool tl0=TL(p1x,p1y,p2x,p2y), tl1=TL(p2x,p2y,p0x,p0y), tl2=TL(p0x,p0y,p1x,p1y);
        double A=-area;
        long cov=0, pass=0;
        vector<unsigned char> cm((size_t)boxA,0);
        for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x){
          double px=x+0.5, py=y+0.5;
          double w0=E(p1x,p1y,p2x,p2y,px,py), w1=E(p2x,p2y,p0x,p0y,px,py), w2=E(p0x,p0y,p1x,p1y,px,py);
          bool in=(w0>0||(w0==0&&tl0))&&(w1>0||(w1==0&&tl1))&&(w2>0||(w2==0&&tl2));
          if(!in) continue;
          cov++; cm[(size_t)(y-y0)*bw+(x-x0)]=1;
          double dd=(w0*iw0+w1*iw1+w2*iw2)/(w0+w1+w2);   // 1/w interpole
          size_t idx=(size_t)y*VW+x; seen[idx]=1;
          if(dd>zb[idx]){ zb[idx]=(float)dd; pass++; owner[idx]=(unsigned char)rm; idb[idx]=gTri; }
        }
        tC[gTri]=cov; S.covered+=cov; S.depthPassed+=pass; roomDrawn[rm]++; roomCovered[rm]+=cov; roomPassed[rm]+=pass;
        if(cov==0)S.cov0++; else if(cov==1)S.cov1++; else if(cov<=4)S.cov24++; else S.cov5++;
        int c = boxA==1?0: boxA<=4?1: boxA<=16?2: boxA<=64?3: boxA<=256?4: boxA<=1024?5: boxA<=8192?6:7;
        S.cls[c]++; S.clsTight[c]+=boxA; S.clsCov[c]+=cov;
        if(boxA>=64 && double(cov)/boxA<0.25) S.thin++;
        // virgule fixe (Giesen) : memes sommets arrondis a 2^-b pixel, aretes ENTIERES exactes
        for(int bits: {8,4}){
          const long long Sc=1LL<<bits; auto R=[&](double v){ return (long long)llround(v*Sc); };
          long long q0x=R(p0x),q0y=R(p0y),q1x=R(p1x),q1y=R(p1y),q2x=R(p2x),q2y=R(p2y);
          auto EI=[&](long long ax,long long ay,long long bx,long long by,long long px,long long py){ return (ax-px)*(by-py)-(ay-py)*(bx-px); };
          auto TLI=[&](long long ax,long long ay,long long bx,long long by){ long long ex=bx-ax,ey=by-ay; return (ey==0&&ex<0)||(ey>0); };
          bool f0=TLI(q1x,q1y,q2x,q2y), f1=TLI(q2x,q2y,q0x,q0y), f2=TLI(q0x,q0y,q1x,q1y);
          long cf=0, df=0;
          for(int y=max(0,y0-1);y<min(VH,y1+1);++y) for(int x=max(0,x0-1);x<min(VW,x1+1);++x){
            long long pxx=(long long)x*Sc+Sc/2, pyy=(long long)y*Sc+Sc/2;
            long long a0=EI(q1x,q1y,q2x,q2y,pxx,pyy), a1=EI(q2x,q2y,q0x,q0y,pxx,pyy), a2=EI(q0x,q0y,q1x,q1y,pxx,pyy);
            bool in=(a0>0||(a0==0&&f0))&&(a1>0||(a1==0&&f1))&&(a2>0||(a2==0&&f2));
            bool inF=false; if(x>=x0&&x<x1&&y>=y0&&y<y1) inF=cm[(size_t)(y-y0)*bw+(x-x0)];
            cf+=in; if(in!=inF) df++;
          }
          if(bits==8){ S.fxCov8+=cf; S.fxDiff8+=df; } else { S.fxCov4+=cf; S.fxDiff4+=df; }
        }
        S.fxCovFloat+=cov;
        if(bw==1&&bh==1){ S.pix1++; S.scalarTight+=boxA; tS[gTri]=boxA; continue; }
        if(bw<3){ S.narrow++; S.scalarTight+=boxA; tS[gTri]=boxA; continue; }
        S.simdTight+=boxA;
        // chemin AVX2 (C2) : blocs de 8 depuis x0, un par ligne
        S.simd++;
        int nb=(bw+7)/8; tB[gTri]=(long)nb*bh;
        for(int y=0;y<bh;++y) for(int b=0;b<nb;++b){
          int c0=0; for(int l=0;l<8;++l){ int xx=b*8+l; if(xx<bw) c0+=cm[(size_t)y*bw+xx]; }
          S.blocks++; if(c0==0)S.blocksEmpty++; if(c0==8)S.blocksFull++; S.lanesCovered+=c0;
        }
        // hierarchie : tuiles TWxTH alignees sur (x0,y0), rejet / acceptation triviaux par les 4 coins de centres
        {
          static const int TW[5]={8,8,16,16,32}, TH[5]={4,8,8,16,32};
          for(int cfg=0;cfg<5;++cfg){
            for(int ty=0;ty<bh;ty+=TH[cfg]) for(int tx=0;tx<bw;tx+=TW[cfg]){
              S.tt[cfg]++;
              int rows=min(TH[cfg],bh-ty), cols=min(TW[cfg],bw-tx);
              double xs[2]={x0+tx+0.5, x0+tx+TW[cfg]-0.5}, ys[2]={y0+ty+0.5, y0+ty+TH[cfg]-0.5};
              bool rej=false, acc=true;
              for(int e=0;e<3&&!rej;++e){
                double mn=1e300,mx=-1e300;
                for(int i=0;i<2;++i)for(int j=0;j<2;++j){
                  double w = e==0?E(p1x,p1y,p2x,p2y,xs[i],ys[j]): e==1?E(p2x,p2y,p0x,p0y,xs[i],ys[j]): E(p0x,p0y,p1x,p1y,xs[i],ys[j]);
                  mn=min(mn,w); mx=max(mx,w);
                }
                if(mx<0) rej=true; if(!(mn>0)) acc=false;
              }
              bool full = rows==TH[cfg]&&cols==TW[cfg];
              if(rej) S.trj[cfg]++;
              else if(acc&&full){ S.tac[cfg]++; S.tFillBlk[cfg]+= (long)rows*(cols/8); }
              else { S.tpa[cfg]++; S.tEdgeBlk[cfg]+= (long)rows*((cols+7)/8); }
            }
          }
        }
      }
    }
    for(int i=0;i<VW*VH;++i){ S.vpPixels++; if(!seen[i]) S.holes++; }
    printf("\n== %s  pos=(%.1f,%.1f,%.1f) yaw=%.0f ==\n",cam.name,cam.pos.x,cam.pos.y,cam.pos.z,cam.yaw);
    printf("submitted %ld  clipped %ld  front %ld  back %ld (%.1f%% des sorties)  earlyRej %ld  drawn %ld\n",S.submitted,S.clipped,S.front,S.back,100.0*S.back/(S.front+S.back),S.earlyRej,S.drawn);
    printf("TrisCov0/1/2-4/5+ = %ld / %ld / %ld / %ld   pix1(1x1) %ld   simd(w>=3) %ld\n",S.cov0,S.cov1,S.cov24,S.cov5,S.pix1,S.simd);
    printf("Tight %ld  Covered %ld  DepthPassed %ld   Covered/Tight %.1f%%   DepthPassed/Covered %.1f%%   Tight/tri %.1f\n",S.tight,S.covered,S.depthPassed,100.0*S.covered/max(1L,S.tight),100.0*S.depthPassed/max(1L,S.covered),double(S.tight)/max(1L,S.drawn));
    const char* nm[8]={"=1","2-4","5-16","17-64","65-256","257-1024","1025-8192",">8192"};
    printf("classes de boite (px) : "); for(int i=0;i<8;++i) printf("[%s: %ld tris, cov/tight %.0f%%] ",nm[i],S.cls[i],S.clsTight[i]?100.0*S.clsCov[i]/S.clsTight[i]:0.0); printf("\n");
    printf("thin(boite>=64 et cov/tight<25%%) %ld\n",S.thin);
    printf("C2 blocs : %ld  vides %.1f%%  pleins %.1f%%  voies couvertes/voies %.1f%%   scalaire(tight) %ld\n",S.blocks,100.0*S.blocksEmpty/max(1L,S.blocks),100.0*S.blocksFull/max(1L,S.blocks),100.0*S.lanesCovered/max(1L,8*S.blocks),S.scalarTight);
    printf("tuiles 8x8 (triangles SIMD) : %ld  rejet %.1f%%  acceptation %.1f%%  partielles %.1f%%   lignes d'aretes %ld -> hierarchie %ld (+%ld lignes de remplissage direct)\n",S.tiles,100.0*S.tReject/max(1L,S.tiles),100.0*S.tAccept/max(1L,S.tiles),100.0*S.tPartial/max(1L,S.tiles),S.blocks,S.tileRowsEdge,S.tileRowsFill);
    { char fn[96]; snprintf(fn,96,"%s.ppm",cam.name); FILE*o=fopen(fn,"wb"); fprintf(o,"P6 %d %d 255\n",VW,VH); for(int i=0;i<VW*VH;++i){ unsigned char c[3]; if(!seen[i]){c[0]=255;c[1]=0;c[2]=255;} else {unsigned char g=(unsigned char)min(255.0,zb[i]*40*255.0/ (1.0+0)); c[0]=c[1]=c[2]=g;} fwrite(c,1,3,o);} fclose(o);} printf("JSON {\"cam\":\"%s\",\"submitted\":%ld,\"clipped\":%ld,\"front\":%ld,\"back\":%ld,\"earlyRej\":%ld,\"drawn\":%ld,\"cov0\":%ld,\"cov1\":%ld,\"cov24\":%ld,\"cov5\":%ld,\"pix1\":%ld,\"narrow\":%ld,\"simd\":%ld,\"tight\":%ld,\"covered\":%ld,\"depthPassed\":%ld,\"simdTight\":%ld,\"scalarTight\":%ld,\"blocks\":%ld,\"blocksEmpty\":%ld,\"blocksFull\":%ld,\"lanes\":%ld,\"tiles\":%ld,\"tReject\":%ld,\"tAccept\":%ld,\"tPartial\":%ld,\"tileRowsEdge\":%ld,\"tileRowsFill\":%ld,\"thin\":%ld,\"holes\":%ld,\"tt\":[%ld,%ld,%ld,%ld,%ld],\"trj\":[%ld,%ld,%ld,%ld,%ld],\"tac\":[%ld,%ld,%ld,%ld,%ld],\"tpa\":[%ld,%ld,%ld,%ld,%ld],\"tEdgeBlk\":[%ld,%ld,%ld,%ld,%ld],\"tFillBlk\":[%ld,%ld,%ld,%ld,%ld],\"fx\":[%ld,%ld,%ld,%ld,%ld],\"cls\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],\"clsCov\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld],\"clsTight\":[%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld]}\n",cam.name,S.submitted,S.clipped,S.front,S.back,S.earlyRej,S.drawn,S.cov0,S.cov1,S.cov24,S.cov5,S.pix1,S.narrow,S.simd,S.tight,S.covered,S.depthPassed,S.simdTight,S.scalarTight,S.blocks,S.blocksEmpty,S.blocksFull,S.lanesCovered,S.tt[1],S.trj[1],S.tac[1],S.tpa[1],S.tEdgeBlk[1],S.tFillBlk[1],S.thin,S.holes,S.tt[0],S.tt[1],S.tt[2],S.tt[3],S.tt[4],S.trj[0],S.trj[1],S.trj[2],S.trj[3],S.trj[4],S.tac[0],S.tac[1],S.tac[2],S.tac[3],S.tac[4],S.tpa[0],S.tpa[1],S.tpa[2],S.tpa[3],S.tpa[4],S.tEdgeBlk[0],S.tEdgeBlk[1],S.tEdgeBlk[2],S.tEdgeBlk[3],S.tEdgeBlk[4],S.tFillBlk[0],S.tFillBlk[1],S.tFillBlk[2],S.tFillBlk[3],S.tFillBlk[4],S.fxCovFloat,S.fxCov8,S.fxDiff8,S.fxCov4,S.fxDiff4,S.cls[0],S.cls[1],S.cls[2],S.cls[3],S.cls[4],S.cls[5],S.cls[6],S.cls[7],S.clsCov[0],S.clsCov[1],S.clsCov[2],S.clsCov[3],S.clsCov[4],S.clsCov[5],S.clsCov[6],S.clsCov[7],S.clsTight[0],S.clsTight[1],S.clsTight[2],S.clsTight[3],S.clsTight[4],S.clsTight[5],S.clsTight[6],S.clsTight[7]);
    { vector<char> vs(gTri+1,0); long vis=0; for(long id: idb) if(id>0 && !vs[id]){ vs[id]=1; vis++; } long hb=0,hc=0,hs=0,ht=0, vb=0,vc=0,vsx=0; for(long i=1;i<=gTri;++i){ if(!vs[i]){ ht++; hb+=tB[i]; hc+=tC[i]; hs+=tS[i]; } else { vb+=tB[i]; vc+=tC[i]; vsx+=tS[i]; } } printf("VISIBLE {\"cam\":\"%s\",\"drawn\":%ld,\"visibleTris\":%ld,\"hiddenTris\":%ld,\"hiddenBlocks\":%ld,\"hiddenCov\":%ld,\"hiddenScalarPx\":%ld,\"visibleBlocks\":%ld,\"visibleCov\":%ld,\"visibleScalarPx\":%ld}\n",cam.name,S.drawn,vis,ht,hb,hc,hs,vb,vc,vsx); }
    { long own[8]={0}; for(int i=0;i<VW*VH;++i) if(owner[i]<8) own[owner[i]]++; printf("ROOMS {\"cam\":\"%s\",\"drawn\":[%ld,%ld,%ld,%ld,%ld,%ld],\"covered\":[%ld,%ld,%ld,%ld,%ld,%ld],\"passed\":[%ld,%ld,%ld,%ld,%ld,%ld],\"owned\":[%ld,%ld,%ld,%ld,%ld,%ld],\"back\":[%ld,%ld,%ld,%ld,%ld,%ld]}\n",cam.name,roomDrawn[0],roomDrawn[1],roomDrawn[2],roomDrawn[3],roomDrawn[4],roomDrawn[5],roomCovered[0],roomCovered[1],roomCovered[2],roomCovered[3],roomCovered[4],roomCovered[5],roomPassed[0],roomPassed[1],roomPassed[2],roomPassed[3],roomPassed[4],roomPassed[5],own[0],own[1],own[2],own[3],own[4],own[5],roomBack[0],roomBack[1],roomBack[2],roomBack[3],roomBack[4],roomBack[5]); }
    printf("pixels jamais couverts (fissures ou vide) : %ld / %ld\n",S.holes,S.vpPixels);
  }
}
