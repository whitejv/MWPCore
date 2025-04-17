# MWPCore
Milano Water Project Core Functions

## ** IMPORTANT ** Install MWPEnv Project Prior to Installing MWPCore

## Install the Project
- -> git clone https://github.com/whitejv/MWPCore.git

## Configure the Project
- -> cd ~
- -> cd MWPCore
  
### CMake Process
- -> cmake -S . (to rebuild the Root Makefile
- -> make clean
- -> make depend
- -> make
- -> make install
- -> cd bin
- -> ls -al (check the dates)

## Add to Cron for Start on Reboot
- -> crontab -e
- -> add the following line to the bottom of the cron file:
- ---> @reboot sleep 20 && bash /home/pi/MWPCore/h2o.sh
- ---> @reboot sleep 10 && . /home/pi/.profile && /home/pi/telegraf/telegraf --config /home/pi/.telegraf/telegraf.conf > >
- ---> #
- ---> # Stop all processes at 1:02 AM daily with logging
- ---> 2 1 * * * /home/pi/MWPCore/kill_h2o.sh >/dev/null 2>&1
- ---> #
- ---> # Start all processes at 1:04 AM daily with logging
- ---> 4 1 * * * /home/pi/MWPCore/h2o.sh >/dev/null 2>&1
- -> cd MWPCore
- -> chmod +x h2o.sh
- -> reboot
