#!/bin/bash
for i in `ipcs |grep nobody |awk '{print $2}'` ; do ipcrm -s $i ; done
