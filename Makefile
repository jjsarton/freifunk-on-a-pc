all: install

install:
	@cd c-src;make;make install
	@mkdir etc/resspondd.d
	@cp c-src/node.d/* etc/resspondd.d
	@echo please build fastd configuration
	@echo and edit bin/conf.sh


distclean:
	@cd c-src;make uninstall
	@if [ -e etc/resspondd.d ];then rm -fr etc/resspondd.d;fi
	@if [ -e etc/fastd.d ];then rm -fr etc/fastd.d;fi
	@if [ -e etc/fastd.conf ];then rm etc/fastd.conf;fi 
