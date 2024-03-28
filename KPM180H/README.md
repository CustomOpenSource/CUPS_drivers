# CUSTOM KPM180H CUPS driver

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
        sudo chmod +x bin/rastertoKPM180H


- Copy the result 

        sudo cp bin/rastertoKPM180H /usr/lib/cups/filter


- Create a CUSTOM folder and copy the ppd file there

        sudo mkdir /usr/share/cups/model/CUSTOM
        cd ppd/
        gzip -k KPM180H.ppd
        sudo cp KPM180H.ppd.gz /usr/share/cups/model/CUSTOM
        gzip -k KPM180HCUT.ppd
        sudo cp KPM180HCUT.ppd.gz /usr/share/cups/model/CUSTOM


- Restart the CUPS server

        sudo service cups restart


### Notes

Compilation is straightforward and should remain identical on all system.

Installation, on the other hand, may vary. Some system place the filter executable and ppd file in different directories, so you may need to check your specific system before copying the files. One way to do that is to check the CUPS' conf file

    cat /etc/cups/cupsd.conf 

## Using the driver

You should now be able to install your printer from the web interface

    http://localhost:631

or by executing a command similar to

    sudo lpadmin -p KPM180H -E -v "usb://CUSTOM%20Engineering/KPM180-H?serial=KPM180-H_PRN_Num.:_0" -P /usr/share/cups/model/CUSTOM/KPM180H.ppd.gz
    sudo lpadmin -p KPM180HCUT -E -v "usb://CUSTOM%20Engineering/KPM180-H%20CUT?serial=KPM180-H_PRN_Num.:_0" -P /usr/share/cups/model/CUSTOM/KPM180HCUT.ppd.gz

Use KPM180HCUT.ppd if the printer is equipped with cutter, otherwise use KPM180H.ppd
