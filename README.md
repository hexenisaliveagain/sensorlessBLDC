# sensorlessBLDC
Sensorless BLDC driver project that aims to create cheap, simple, powerfull and versitile BLDC motor controller. Project consists of source code that runs on ATMEGA 328P and uses internal comparator module to guess right moent for cummutation execution. Projcet is curently IN WORKS.



PROJECT IS SUPPLIED AS IS AND I SHALL TAKE NO RESPONSIBILITY FOR ANY PROBLEMS CAUSED BY USING MY CODE, SCHEMATIC, PCB DESIGN OR ANY WORK I HAVE POSTED HERE. PROJECT IS FULLY CREATED BY MACIEJ GAŁDA FOR HIS EE DEGREE ON POLITECHNIKA RZESZOWSKA IM. INGACEGO ŁUKASIEWICZA IN ACADEMIC YEAR OF 2022/2023

Project now keeps the same Arduino NANO / ATmega328P hardware and still uses sensorless six-step BLDC commutation with the internal analog comparator, but the firmware no longer relies only on a fixed PWM bump when the motor slows down. The commutation ISR measures the time between BEMF zero crossings, ignores comparator spikes during a short blanking window, debounces the comparator output, delays commutation by approximately 30 electrical degrees, and uses a lightweight PI loop to raise or lower PWM when the measured BEMF period drifts from the target period. A software timeout detects loss of synchronization and attempts a controlled relaunch before stopping the motor. The project still does not use motor current monitoring, so current limiting and true torque control require a future hardware revision. Motor can be controlled by sending commands by UART interface from the host device connected by USB. Comments are written in Polish as of now.
Right now current release of the driver FW has been proven to drive just fine motors for RC planes. In other applications lack of motor current measurement and simple control loop for the motor has been proven to be not good enough. Next HW revision should sport highside and in line motor current measurement for more control over the driver unit itself and driven motor.

Next goal of the project is to make driver suitable for any BLDC motor application.

PCB Renders:

![1](https://user-images.githubusercontent.com/101871819/222913972-fe1ccbfc-8907-4b3b-8d01-60e5f7b51d97.PNG)
![2](https://user-images.githubusercontent.com/101871819/222913977-8684dad2-3bb3-4538-912d-1dd7ec3f00e9.PNG)
