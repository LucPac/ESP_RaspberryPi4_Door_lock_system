# SETUP LIBRARY

**STEP 1: UPDATE RASPBERRY PI**  
`sudo apt-get update`  
`sudo apt-get upgrade -y`  
**STEP 2: INSTALL SYSTEM PACKAGES (IMPORTANT)**  
`sudo apt-get install python3-pip python3-venv -y`  
`sudo apt-get install libgtk-3-dev pkg-config libgl1-mesa-glx -y`  
**STEP 3: MOVE INTO YOUR PROJECT DIRECTORY**  
`cd DoAn2`  
**STEP 4: CREATE AND ACTIVATE VIRTUAL ENVIRONMENT**  
`python3 -m venv env --system-site-packages`  
Activate the environment  
`source env/bin/activate`  
**STEP 5: INSTALL PYTHON LIBRARIES (INTO VIRTUAL ENV)**  
This fixes the Numpy 1.x vs 2.x conflict  
We install Numpy 1.x (stable) FIRST  
`pip install "numpy<2"`  
`pip install paho-mqtt`  

**---------------------------------------------------------------------------------------**

# SETUP BROKER MQTT  

**STEP 1: INSTALL MOSQUITTO BROKER AND CLIENTS**  
`sudo apt-get install mosquitto mosquitto-clients -y`  
**STEP 2: ENABLE BROKER WHEN RASPBERRY START**  
`sudo systemctl enable mosquitto.service`  
`sudo systemctl start mosquitto.service`  
`sudo nano /etc/mosquitto/mosquitto.conf`  

Past this script in the latest line

```
listener 1883
allow_anonymous true
```

`sudo systemctl restart mosquitto`  
**STEP 3: CHECK STATUS BROKER**  
`systemctl status mosquitto`  
**Note:**  
Turn off immediately: `sudo systemctl stop mosquitto.service`  
Turn off auto run: `sudo systemctl disable mosquitto.service`  
Start auto run: `sudo systemctl enable mosquitto.service`  

**---------------------------------------------------------------------------------------**

# SETUP AUTORUN FILE PYTHON 03_FACE_RECOGNITION.PY

**STEP 1: MAKE SERVICE FILE**  
`sudo nano /etc/systemd/system/face_recognition.service`  

Paste this script into face_recognition.service

```
[Unit]
Description=Dich vu Nhan dien khuon mat
After=network-online.target
Wants=network-online.target

[Service]
User=pi
Group=pi
WorkingDirectory=/home/pi/DoAn2
ExecStart=/home/pi/DoAn2/env/bin/python /home/pi/DoAn2/03_face_recognition.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

**STEP 2: ACTIVE SERVICE**

Read new service file: `sudo systemctl daemon-reload`  
Enable service file: `sudo systemctl enable face_recognition.service`  
Start service file: `sudo systemctl start face_recognition.service`  
Check status service file: `systemctl status face_recognition.service`  
Show log in realtime: `journalctl -u face_recognition.service -f`

**Note:**  
Turn off immediately: `sudo systemctl stop face_recognition.service`  
Turn off auto run: `sudo systemctl disable face_recognition.service`  
Start auto run: `sudo systemctl enable face_recognition.service`

**---------------------------------------------------------------------------------------**

# CHANGE WIFI CONNECT

``nmcli dev wifi list``  
``nmcli dev wifi connect "The Coffee House" password "nguyentandat"``  
``nmcli con show``  
