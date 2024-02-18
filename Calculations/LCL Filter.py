from scipy import signal
import numpy as np
import matplotlib.pyplot as plt



Li = (0.001 * 3)
Ri = (3.5*3)

Lg = (0.001 * 2)
Rg = (3.5*2)

Cf = (0.000001 * 4)
Rf = 0.000000000001



H_LCL1 = signal.TransferFunction([Rf*Cf, 1], [(Lg*Li*Cf), (Cf*(Lg*(Rf+Ri) + Li*(Rf+Rg))), (Lg+Li+Cf*(Rf*Rg + Rf*Ri + Rg*Ri)), Rg+Ri])

w = np.linspace(0.1, 30, num=500, endpoint=True)

w, mag, phase = signal.bode(H_LCL1, w=w)

w = w*2*np.pi

plt.figure()
plt.title("Mag")
#plt.semilogx(w, mag)    # Bode magnitude plot
plt.plot(w,mag)
plt.grid()
plt.savefig("mag.png")

plt.figure()
plt.title("Phase")
#plt.semilogx(w, phase)  # Bode phase plot
plt.plot(w, phase)
plt.grid()
plt.savefig("phase.png")
plt.show()
