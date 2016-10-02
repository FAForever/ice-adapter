#!/usr/bin/env python
# -*- coding: utf-8 -*-
import requests

sdpserver_address = "http://127.0.0.1:5000/"
faf_ice_adapter_address = "http://127.0.0.1:8001/"
my_host_id=1234

r = requests.get(faf_ice_adapter_address + "create_game")
print(r.text)
