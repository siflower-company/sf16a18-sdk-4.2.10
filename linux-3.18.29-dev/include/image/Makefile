#this file is for build uImage
$(info build image start--$(CURDIR)--makelevel=$(MAKELEVEL))

include $(CURDIR)/image.mk

image:
	$(call Image/Prepare)
	$(call Image/Prepare-ramfs-img)
	$(call Image/MakeSysupgradeBinary)
	$(call Image/CleanTmp)

clean:
	$(call Image/Clean)

all: image
.PHONY: all
