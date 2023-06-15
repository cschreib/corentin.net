# How to make your own website in C++ on a RaspberryPI

## Configure your network

* Get a static public IP address for your router (you need to ask your internet service provider for this, some of them provide this service for free).
* Get a static local IP for your RaspberryPI (you need to enter your router's configuration to do this).
* Forward the ports 80 and 443 (for HTTP and HTTPS) to your RaspberryPI (again, this is done in the router's interfae).
* Open these ports in the firewall of your RaspberryPI, if you have one (no firewall is installed by default in Raspbian).
* Rent a domain name. This can take a couple days to be effective, so plan ahead.

## Configure your SSL certifiates on the RaspberryPI

* Install Certbot
```bash
sudo apt install certbot
```
* Request certificate
```bash
sudo certbot certonly --standalone
```
* When asked, enter the domain names of your website. If your domain is ``mywebsite.com``, enter ``mywebsite.com,www.mywebsite.com`` (and any other combination you plan to use, separated by a comma).
* The tool will validate your server, and if you configured your network properly (steps above) you should obtain your SSL certificate. It saves the various needed files in ``/etc/letsencrypt/live/mywebsite.com/*`` (with ``mywebsite.com`` replaced with your domain name).

## Build the HTTP server software

### Native

Build:
```bash
mkdir build_native
cd build_native
cmake ..
make install
# output in build_native/website
```

### Cross-compile

Set up (inspired from [this](https://solarianprogrammer.com/2019/05/04/clang-cross-compiler-for-raspberry-pi/)):
```bash
sudo apt install crossbuild-essential-armhf
```

Build:
```bash
mkdir build_pi
cd build_pi
cmake .. -DCMAKE_CXX_COMPILER="arm-linux-gnueabihf-g++"
make
# output in build_pi/website
```

## Setup ``systemd`` to run server in background

* Using ``sudo``, create a new file in ``/etc/systemd/system`` called, for example, ``myserver.service``, and fill it with the following (update the path to the web server executable):
```ini
[Unit]
Description=My webserver

[Service]
ExecStart=/<absolute-path-to-your-server>/<your-server-executable-name>
Restart=always

[Install]
WantedBy=multi-user.target
```

* Make this file executable:
```bash
sudo chmod +x myserver.service
```

* Enable running this on startup:
```bash
sudo systemctl enable myserver.service
```

* Start it now:
```bash
sudo systemctl start myserver.service
```

* Note: for this to work properly, your executable should run an infinite loop while the server is running, and only terminate when receiving the SIGTERM or SIGINT signals. In particular, using ``std::getline(std::cin, ...)`` or any other similar IO trick to create the infinite wait will not work (systemd will continuously kill and restart the server).

* To restart the server:
```bash
sudo systemctl restart myserver.service
```

* To stop the server:
```bash
sudo systemctl stop myserver.service
```

* To stop the server permanently (so that it does not start again on next system restart):
```bash
sudo systemctl disable myserver.service
```
