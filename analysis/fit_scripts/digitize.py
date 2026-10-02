from PIL import Image
import numpy as np, csv
from scipy import ndimage as ndi
a=np.array(Image.open('/mnt/user-data/uploads/Tone_images.png').convert('L')).astype(int)
panels=[(29,687,55,472),(783,1440,55,472),(1545,2202,55,472),(29,687,769,1186),(783,1440,769,1186),(1545,2202,769,1186)]
xt=[ (141,278,415,553,635),(894,1031,1169,1306,1388),(1656,1794,1931,2068,2150)]
hy_top=[(99,404),(814,1118)]   # +6 and -24 gridlines
ticks_f=np.array([10,100,1e3,1e4,2e4])
def xpix2f(x,t):
    t=np.array(t,float); lf=np.log10(ticks_f)
    # piecewise log-linear between ticks, extrapolating with end segments
    if x<t[0]: s=(lf[1]-lf[0])/(t[1]-t[0]); return 10**(lf[0]+(x-t[0])*s)
    if x>t[-1]: s=(lf[-1]-lf[-2])/(t[-1]-t[-2]); return 10**(lf[-1]+(x-t[-1])*s)
    return 10**np.interp(x,t,lf)
def ypix2db(y,top,bot): return 6+(y-top)*(-24-6)/(bot-top)
out={}
for k,(x0,x1,y0,y1) in enumerate(panels):
    t=xt[k%3]; top,bot=hy_top[k//3]
    sub=a[y0:y1,x0:x1]<90
    thick=ndi.binary_opening(sub,structure=np.ones((5,5)))
    xs=[];ys=[]
    for x in range(t[0]-20-x0, x1-x0):
        col=thick[:,x]
        yy=np.where(col)[0]
        yy=yy[(yy+y0>top-25)&(yy+y0<bot+25)]
        if len(yy)==0: continue
        xs.append(x+x0); ys.append(yy.mean()+y0)
    f=np.array([xpix2f(x,t) for x in xs]); db=np.array([ypix2db(y,top,bot) for y in ys])
    out[k+1]=(f,db,np.array(xs),np.array(ys))
    print(k+1,len(xs),f.min().round(2),f.max().round(0),db.min().round(1),db.max().round(1))
np.save('tone/dig.npy',out,allow_pickle=True)
# csv: resampled on log grid
grid=np.logspace(np.log10(10),np.log10(20000),121)
with open('tone/tone_curves_digitized.csv','w',newline='') as fh:
    w=csv.writer(fh); w.writerow(['freq_hz']+[f'curve{k}_db' for k in range(1,7)])
    cols=[]
    for k in range(1,7):
        f,db,_,_=out[k]; cols.append(np.interp(np.log10(grid),np.log10(f),db))
    for i,g in enumerate(grid): w.writerow([round(g,2)]+[round(c[i],2) for c in cols])
