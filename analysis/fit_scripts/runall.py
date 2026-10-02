import sys, pickle, numpy as np
import fit2
fs=float(sys.argv[1]); Ns={1:4,2:5,3:3,4:4,5:2,6:3}
for k,N in Ns.items():
    x,e,fg,tg=fit2.fit(k,fs,N,restarts=40,seed=k*10+N)
    pickle.dump((x,N,float(np.sqrt(np.mean(e**2))),float(np.abs(e).max())),open(f'tone/final_{int(fs)}_{k}.pkl','wb'))
    print(int(fs),k,N,'rms %.3f max %.3f'%(np.sqrt(np.mean(e**2)),np.abs(e).max()),flush=True)
