import numpy as np, json, sys
from scipy import signal, optimize
out=np.load('tone/dig.npy',allow_pickle=True).item()

# --- notch tip correction for curve 2 (stroke centroid is biased at the V tip)
f2,db2,xs2,ys2=out[2]
def line_fit(xlo,xhi):
    m=(xs2>=xlo)&(xs2<=xhi); return np.polyfit(xs2[m],ys2[m],1)
# use steep side segments (pixel columns), excluding the blunt tip itself
L=line_fit(1129,1140); R=line_fit(1158,1166)
xt=(R[1]-L[1])/(L[0]-R[0]); yt=L[0]*xt+L[1]
top,bot=99,404
tip_db=6+(yt-top)*(-30)/(bot-top)
t=(1031,1169) # 100Hz,1kHz ticks for panel 2 (894,1031,1169,1306,1388)
tip_f=10**np.interp(xt,[894,1031,1169,1306,1388],np.log10([10,100,1e3,1e4,2e4]))
print('notch tip: %.0f Hz, %.1f dB'%(tip_f,tip_db))
keep=(xs2<xt-8)|(xs2>xt+8)
f2n=np.concatenate([f2[keep],[tip_f]]); d2n=np.concatenate([db2[keep],[tip_db]])
o=np.argsort(f2n); out[2]=(f2n[o],d2n[o],None,None)

def target(k,grid): 
    f,db=out[k][0],out[k][1]
    return np.interp(np.log10(grid),np.log10(f),db)

def sos_resp(p,w,N):
    H=np.ones_like(w,dtype=complex)
    for i in range(N):
        b1,b2,k1,k2=p[4*i:4*i+4]
        a2=k2; a1=k1*(1+k2)
        z=np.exp(-1j*w)
        H*= (1+b1*z+b2*z**2)/(1+a1*z+a2*z**2)
    return H

def fit(k,fs,N,restarts=60,seed=0):
    rng=np.random.default_rng(seed)
    fgrid=np.logspace(np.log10(10),np.log10(min(20000,0.45*fs)),400)
    w=2*np.pi*fgrid/fs
    tgt=target(k,fgrid)
    best=None
    def res(q):
        p=q[:-1]; g=q[-1]
        H=sos_resp(p,w,N)
        return 20*np.log10(np.abs(H)+1e-12)+g-tgt
    for r in range(restarts):
        p0=[]
        for i in range(N):
            b1=rng.uniform(-2,0); b2=rng.uniform(0,1) if rng.random()<.7 else rng.uniform(-1,0)
            k1=rng.uniform(-.99,.99); k2=rng.uniform(-.99,.99)
            p0+= [b1,b2,k1,k2]
        q0=np.array(p0+[0.0])
        lb=[]; ub=[]
        for i in range(N): lb+=[-2.2,-1.2,-.9999,-.9999]; ub+=[2.2,1.2,.9999,.9999]
        lb+=[-40]; ub+=[40]
        try:
            s=optimize.least_squares(res,q0,bounds=(lb,ub),method='trf',max_nfev=400,x_scale=1.0)
        except Exception as e: continue
        if best is None or s.cost<best[0].cost: best=(s,)
    s=best[0]; e=res(s.x)
    return s.x,e,fgrid,tgt

if __name__=='__main__':
    fs=float(sys.argv[1]) if len(sys.argv)>1 else 48000
    Ns={1:3,2:4,3:3,4:3,5:2,6:3}
    results={}
    for k,N in Ns.items():
        x,e,fg,tg=fit(k,fs,N)
        print(k,N,'rms %.3f dB  max %.3f dB'%(np.sqrt(np.mean(e**2)),np.abs(e).max()),flush=True)
        results[k]=(x,N)
    np.save(f'tone/fit_{int(fs)}.npy',results,allow_pickle=True)
