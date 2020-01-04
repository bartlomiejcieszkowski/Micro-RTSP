Use it as a component in your esp32-cam project

Provides KConfig that allows selecting camera config, resoltion and port

Known Issues:
 with default esp-idf - careful with ISR stack size - default in esp-idf is 1024,
 this is not enough with esp-camera, increase it to 2048
