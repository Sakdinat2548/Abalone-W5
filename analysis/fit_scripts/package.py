import pickle, json, csv, numpy as np
from scipy import signal
import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
import fit
from fit import out, target
names={1:'1 acoustic/strings/bass/keys',2:'2 bass + mid scoop',3:'3 acoustics/strings/bass/keys',4:'4 acoustics/strings/bass/keys',5:'5 acoustic + electric',6:'6 electric + bass'}
res={}; maxerr={}
for fs in (44100,48000,96000):
    res[fs]={}
    for k in range(1,7):
        x,N,rms,mx=pickle.load(open(f'tone/final_{fs}_{k}.pkl','rb'))
        g=10**(x[-1]/20); sos=[]
        for i in range(N):
            b1,b2,k1,k2=x[4*i:4*i+4]; a2=k2; a1=k1*(1+k2)
            b0=g if i==0 else 1.0
            sos.append([b0, b0*b1, b0*b2, 1.0, a1, a2])
        sos=np.array(sos)
        poles=np.abs(np.roots(sos[0][3:])) if False else max(np.abs(np.roots(s[3:])).max() for s in sos)
        assert poles<1, (fs,k,poles)
        # independent verification with scipy against the digitized curve
        fg=np.logspace(1,np.log10(20000),400)
        _,h=signal.sosfreqz(sos,worN=fg,fs=fs)
        err=20*np.log10(np.abs(h))-target(k,fg)
        res[fs][k]={'sections':len(sos),'sos_b0_b1_b2_a0_a1_a2':sos.tolist(),'max_pole_radius':float(poles),
                    'rms_err_db':round(float(np.sqrt(np.mean(err**2))),3),'max_err_db':round(float(np.abs(err).max()),3)}
json.dump({'note':'Each curve is a cascade of biquads (direct form, a0=1); overall gain is folded into the first section. Fit to curves digitized from the Avalon U5 manual tone-bank graphs, 10 Hz-20 kHz. Errors are vs the digitized curve (dB).',
           'curve_names':names,'fits':{str(fs):{str(k):v for k,v in d.items()} for fs,d in res.items()}},open('/mnt/user-data/outputs/u5_tone_biquads.json','w'),indent=1)
# csv
grid=np.logspace(1,np.log10(20000),121)
with open('/mnt/user-data/outputs/u5_tone_curves_digitized.csv','w',newline='') as fh:
    w=csv.writer(fh); w.writerow(['freq_hz']+[f'curve{k}_db' for k in range(1,7)])
    cols=[target(k,grid) for k in range(1,7)]
    for i,gv in enumerate(grid): w.writerow([round(gv,2)]+[round(c[i],2) for c in cols])
# plot at 48k
fig,ax=plt.subplots(2,3,figsize=(15,8))
fg=np.logspace(1,np.log10(20000),600)
for k in range(1,7):
    a=ax[(k-1)//3][(k-1)%3]
    sos=np.array(res[48000][k]['sos_b0_b1_b2_a0_a1_a2'])
    _,h=signal.sosfreqz(sos,worN=fg,fs=48000)
    a.semilogx(fg,target(k,fg),'k',lw=4,alpha=.35,label='picture (digitized)')
    a.semilogx(fg,20*np.log10(np.abs(h)),'r',lw=1.3,label='biquad fit @48k')
    a.set_ylim(-25,7); a.grid(True,which='both',alpha=.3); a.set_title(names[k]+'  (max err %.2f dB)'%res[48000][k]['max_err_db'],fontsize=9)
    a.set_xticks([10,100,1000,10000,20000]); a.set_xticklabels(['10','100','1k','10k','20k']); a.set_xlim(9,22000)
ax[0][0].legend(loc='lower right',fontsize=8)
plt.tight_layout(); plt.savefig('/mnt/user-data/outputs/u5_tone_fit_check.png',dpi=110)
for fs in res: print(fs,{k:(v['sections'],v['max_err_db']) for k,v in res[fs].items()})
