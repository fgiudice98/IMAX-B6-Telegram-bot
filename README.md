# IMAX B6 telegram bot
Sends the content of the IMAX B6 LCD to your telegram

## Disclaimer
>**I'm not responsible of any damage to you batteries, your ESP32, your charger, yourself or anything.**

>Be kind with your batteries and you charger!

NOT 100% working, sometimes can miss a message or sniff the wrong character.

Works with 80MHz CPU and 40MHz Flash


## Wiring

|ESP32 PIN|IMAX B6 LCD|
|---|---|
|32|E|
|33|RS|
|26|D4|
|27|D5|
|14|D6|
|13|D7|
|GND|VSS|

Except GND all pin are connected with a voltage divider like

     v IMAX B6 LCD 
     |
    [#] 15k
    [#]
     +--[##]--GND
     |  22k
     |
     v ESP32 PIN

**I strongly recommend to power the ESP32 from an USB source**

## Examples
#### Text mode
![Text example](text_ex.png?raw=true "Text example")
#### Image mode
![Image example](im_ex.png?raw=true "Image example")