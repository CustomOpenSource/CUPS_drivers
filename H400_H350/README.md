# CUSTOM H400_H350 CUPS driver

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
        sudo chmod +x bin/rastertoH400_H350


- Copy the result 

        sudo cp bin/rastertoH400_H350 /usr/lib/cups/filter


- Create a HP folder and copy the ppd file there

        sudo mkdir /usr/share/cups/model/HP
        cd ppd/
        gzip H400_H350.ppd
        sudo cp H400_H350.ppd.gz /usr/share/cups/model/HP


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

    sudo lpadmin -p HP_H400 -E -v "usb://HP/H400?serial=H400_PRN_NUM.0" -P /usr/share/cups/model/HP/H400_H350.ppd.gz
