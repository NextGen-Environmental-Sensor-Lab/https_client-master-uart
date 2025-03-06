# https_client-master-uart
Working with **Actinius Icarus nrf9160** board. This code merges the **https_client-master** and **echo_bot_2** projects to have input from UART0 (the console) POSTed to Google Sheets. The **https_client-master** code connects to Google Sheets (file Telit Test) and sends dummy data. The **echo_bot_2** gets UART0 and UART1 talking through the board. I used the console (UART0) and connected a USB-2-UART (like Adafruit 954) to UART1 (RX,TX) on Icarus.
 
