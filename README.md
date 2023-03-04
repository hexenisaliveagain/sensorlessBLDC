# sensorlessBLDC
Sensorless BLDC driver project that aims to create cheap, simple, powerfull and versitile BLDC motor controller. Project consists of source code that runs on ATMEGA 328P and uses internal comparator module to guess right moent for cummutation execution. Projcet is curently IN WORKS.



PROJECT IS SUPPLIED AS IS AND I SHALL TAKE NO RESPONSIBILITY FOR ANY PROBLEMS CAUSED BY USING MY CODE, SCHEMATIC, PCB DESIGN OR ANY WORK I HAVE POSTED HERE. PROJECT IS FULLY CREATED BY MACIEJ GAŁDA FOR HIS EE DEGREE ON POLITECHNIKA RZESZOWSKA IM. INGACEGO ŁUKASIEWICZA IN ACADEMIC YEAR OF 2022/2023

Project uses very simple control mechanism. It applies fixed value to tune motor speed when driver finds out that motor has slown down. Meaning that as soon when  load applied to the motor starts to slow it down, then the voltage driving the motor will be rised by fixed value until motor will stay in sync with electronic commutator. Code runs on Arduino NANO with Atmega328P Microcontroller. Project does not use any motor current monitoring as of yet. Motor can be controlled by sending commands by UART interface from the host device connected by USB. Comments are written in Polish as of now.
Right now current release of the driver FW has been proven to drive just fine motors for RC planes. In other applications lack of motor current measurement and simple control loop for the motor has been proven to be not good enough. Next HW revision should sport highside and in line motor current measurement for more control over the driver unit itself and driven motor.

Next goal of the project is to make driver suitable for 

PCB Renders:

![1](https://user-images.githubusercontent.com/101871819/222913972-fe1ccbfc-8907-4b3b-8d01-60e5f7b51d97.PNG)
![2](https://user-images.githubusercontent.com/101871819/222913977-8684dad2-3bb3-4538-912d-1dd7ec3f00e9.PNG)
