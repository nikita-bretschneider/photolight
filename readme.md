# DIY Photo/Video light

## Electrical design overview

The main goal of this project is to design light with variable intensity and color temperature and do this in eficient and affordable way. This light is intended to be used with video/digicine cameras, so there is absolute need to avoid all flickering caused typically by interference between PWM regulator and rolling shutter. The most simple way to achieve this to use linear regulation, which is not efficient. That leads to some overheating problems and similar issues. PWM regulation leads to flickering. The best way to do this is some kind of regulated switched mode DC power supply, but this is imo too complicated to use in DIY design. Maybe three LM2576 step-down converters or similar design will be okay, Idnk, but I tried some kind of a *half way*, which is simple to build (but not to design).

There are three PWM signals generated using the *ATtiny13*. Theese PWMs have some phase features (see firmware sources), which is important to lower current draining from the power supply to the regulator, but the main goal — to remove flickering — is achieved by forcing *switching transistors* into partially linear mode. They are loaded by a huge capacity (electrolytic capacitor battery with 2× 1mF for each transistor), which translates into input (G-S capacity) which together with gate resistors forms low-pass RC filter. This, in fact, forces transistor partially into linear mode where it limits and shapes the charging current adding serial resistance to it, which leads to power loss and heat disipation (this is why I used *IRF510*, they works to 170°C which means relatively small heatsinks), but also to low ripple on the output (I have achieved ripple under 120mV in whole range with all LEDs connected), which causes virtually no flickering, which is to be achieved. 

## Mechanical design overview

### Regulator part

The mechanical part of the regulator is self-explaining. Using two breadboard/bastelboard, one for electronic with transistors, another for capacitor battery. Theese capacitors are loaded by circular currents which leads to lowering their life. Adding more capacitors in paralel would reduce this effect, but capacitors are cheap and can be easily replaced (this is why two boards are used). Then Up/Down switch, Cold/White/Warm switch (select which chain is affected by Up/Down switch) and the last switch is used to switch off two of four *white* LED chains. Simple to use, simple to operate.

### Light

The main board is 300×300×5mm *CEMVIN*. *Cemvin* is composite material containing concrete, vermiculite, cellulose and some another materials, it is durable heat and fire resistant electrical isolant and lightweight. It is easy to glue almost anything on it and it is cheap. I've paid abt 2€ for this board. I bought the LED aluminium profiles with difusors at GME (www.gme.cz), part number *657-161*. They are 1m long, so I bought two of them and cut every to four pieces. They are relatively easy to cut, but when it comes to difusors I ran into a great dificulties. Long story short: cut them using the kitchen knife. LED chains was also bought at GME under part numbers *960-576*, *960-577* and *960-578*.

## Conclusion

There are many ways to build this. I want to inspire people to build simple things themselves. It's sometimes not easy, but it's fun. The firmware is written in C and avaiable under GPL and (imo) is fully commented. The whole thing works fine without any problems. This is not step-by-step guide, this is inspiration only. 