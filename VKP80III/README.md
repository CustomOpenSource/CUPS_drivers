# CUSTOM VKP80III CUPS driver

## Package description

This is the CUPS printer driver package, containing:

- Driver source code, in the src and ppd folders
- Makefile


## Requirements

This software requires that the CUPS server & architecure (see www.cups.org) is 
available on your computer.
This drivers depends on libPtPLm library, which is thus required when compiling.


## Compilation and installation

- Compile page to page support library (libPtPLm) in the parent folder PtPLm

- Copy the output of PtPLm library compilation in a new folder called lib

        mkdir lib
        cp -r ../PtPLm/out/* ./lib

- Compile the executables

        make
        sudo chmod +x bin/rastertoVKP80III


- Copy the result 

        sudo cp bin/rastertoVKP80III /usr/lib/cups/filter


- Create a CUSTOM folder and copy the ppd file there

        sudo mkdir /usr/share/cups/model/CUSTOM
        cd ppd/
        gzip -k VKP80III.ppd
        sudo cp VKP80III.ppd.gz /usr/share/cups/model/CUSTOM


- Copy libPtPLm library to default library location

        sudo cp lib/libPtPLm.so /usr/lib/


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

    sudo lpadmin -p VKP80III -E -v "usb://CUSTOM%20Engineering/VKP80III?serial=VKP80III_PRN_Num.:_0" -P /usr/share/cups/model/CUSTOM/VKP80III.ppd.gz
