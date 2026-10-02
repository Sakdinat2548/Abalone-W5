import numpy as np, sys
from scipy import optimize, signal
import fit as F
from fit import out, target, sos_resp

def init_sections(rng,N,fs):
    p=[]
    for i in range(N):
        fz=10**rng.uniform(np.log10(15),np.log10(0.4*fs)); fp=10**rng.uniform(np.log10(15),np.log10(0.4*fs))
        Qz=10**rng.uniform(-0.5,0.7); Qp=10**rng.uniform(-0.5,0.7)
        wz,wp=2*np.pi*fz,2*np.pi*fp
        b=[1,wz/Qz,wz**2]; a=[1,wp/Qp,wp**2]
        bz,az=signal.bilinear(b,a,fs)
        bz=bz/bz[0]; 
        # reflection coeffs from az
        k2=az[2]; k1=az[1]/(1+az[2])
        p+=[bz[1],bz[2],np.clip(k1,-.999,.999),np.clip(k2,-.999,.999)]
    return np.array(p+[0.0])

def fit(k,fs,N,restarts=200,seed=1):
    rng=np.random.default_rng(seed)
    fgrid=np.logspace(np.log10(10),np.log10(min(20000,0.45*fs)),250)
    w=2*np.pi*fgrid/fs; tgt=target(k,fgrid)
    def res(q):
        H=sos_resp(q[:-1],w,N); return 20*np.log10(np.abs(H)+1e-12)+q[-1]-tgt
    lb=[];ub=[]
    for i in range(N): lb+=[-2.3,-1.3,-.9999,-.9999]; ub+=[2.3,1.3,.9999,.9999]
    lb+=[-60]; ub+=[60]
    best=None
    for r in range(restarts):
        q0=init_sections(rng,N,fs)
        # initial gain match
        e0=res(q0); q0[-1]=-np.mean(e0)
        q0=np.clip(q0,np.array(lb)+1e-6,np.array(ub)-1e-6)
        try: s=optimize.least_squares(res,q0,bounds=(lb,ub),max_nfev=300)
        except Exception: continue
        if best is None or s.cost<best.cost: best=s
    e=res(best.x); return best.x,e,fgrid,tgt

if __name__=='__main__':
    fs=float(sys.argv[1]); k=int(sys.argv[2]); N=int(sys.argv[3])
    x,e,_,_=fit(k,fs,N)
    print(k,N,'rms %.3f max %.3f'%(np.sqrt(np.mean(e**2)),np.abs(e).max()))
