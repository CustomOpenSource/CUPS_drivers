# CUSTOM TG2460-H-EJ CUPS driver

## Package description

This is the CUPS printer driver package, containing:

- Driver source code, in the src and ppd folders
- Makefile


## Requirements

This software requires that the CUPS server & architecure (see www.cups.org) is 
available on your computer


## Compilation and installation

- Compile the executables

        make
        sudo chmod +x bin/rastertoTG2460-H-EJ


- Copy the result 

        sudo cp bin/rastertoTG2460-H-EJ /usr/lib/cups/filter


- Create a CUSTOM folder and copy the ppd file there

        sudo mkdir /usr/share/cups/model/CUSTOM
        cd ppd/
        gzip -k TG2460-H-EJ.ppd
        sudo cp TG2460-H-EJ.ppd.gz /usr/share/cups/model/CUSTOM


- Restart the CUPS server

        sudo service cups restart


### Notes

Compilation is straightforward and should remain idential on all system.

Installation, on the other hand, may vary. Some system place the filter executable and ppd file in different directories, so you may need to check your specific system before copying the files. One way to do that is to check the CUPS' conf file

    cat /etc/cups/cupsd.conf 

## Using the driver

You should now be able to install your printer from the web interface

    http://localhost:631

or by executing a command similar to

    sudo lpadmin -p TG2460-H-EJ -E -v "usb://CUSTOM_SPA/TG2460-H-EJ?serial=TG2460-H-EJ_NUM:_0" -P /usr/share/cups/model/CUSTOM/TG2460-H-EJ.ppd.gz
