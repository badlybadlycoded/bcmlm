#!/bin/sh

# Dependencies
sudo apt install libasio-dev build-essential mailutils

# add bcmlm group
sudo groupadd bcmlm
sudo usermod -aG bcmlm root

# Create global dirs and config files
sudo mkdir -p /var/bcmlm/
sudo chown root:bcmlm /var/bcmlm
sudo chmod g+rwxs /var/bcmlm 
sudo cp confdir/*.yaml /var/bcmlm/
sudo mkdir -p /var/bcmlm/templates/
sudo cp mlm/templates/*.html /var/bcmlm/templates
sudo mkdir -p /var/log/bcmlm/
sudo chown root:bcmlm /var/log/bcmlm 
sudo chmod g+rwxs /var/log/bcmlm 

# setup testlist mailing list
sudo useradd -m -G bcmlm,mail testlist
sudo touch /var/mail/testlist
sudo chown testlist:mail /var/mail/testlist
sudo chmod 660 /var/mail/testlist
sudo touch /var/log/bcmlm/testlist.log
sudo chown testlist:bcmlm /var/log/bcmlm/testlist.log
sudo chmod 664 /var/log/bcmlm/testlist.log

# make mbm
cd mbm; make || exit 255
sudo cp mbm /usr/sbin/bcmbm
sudo chown root:root /usr/sbin/bcmbm
sudo chmod 755 /usr/sbin/bcmbm
sudo chmod a+sx /usr/sbin/bcmbm

# make mlm
cd ../mlm; make || exit 254
sudo cp mlm /usr/sbin/bcmlm
sudo chown root:root /usr/sbin/bcmlm
sudo chmod 755 /usr/sbin/bcmlm
sudo chmod a+sx /usr/sbin/bcmlm

# Add certs to global dir
sudo mkdir -p /var/bcmlm
sudo cp resource/cert.pem /var/bcmlm/cert.pem
sudo cp resource/ec_key.pem /var/bcmlm/ec_key.pem

# Install cert in os key store
sudo openssl x509 -in resource/cert.pem -inform PEM -out /usr/local/share/ca-certificates/bcmlm-ca-cert.crt
sudo update-ca-certificates

# Install as systemd services
cd ..
sudo cp bcmlm.service /etc/systemd/system/
sudo chown root:root /etc/systemd/system/bcmlm.service
sudo chmod 644 /etc/systemd/system/bcmlm.service
sudo cp bcmbm.service /etc/systemd/system/
sudo chown root:root /etc/systemd/system/bcmbm.service
sudo chmod 644 /etc/systemd/system/bcmbm.service