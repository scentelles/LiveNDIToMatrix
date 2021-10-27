import sys
import os
import socket
from pythonosc import dispatcher
from pythonosc import osc_server



hostname = socket.gethostname()
print("hostname : ", hostname)
local_ip = socket.gethostbyname(hostname + ".local")

  
print("local IP : ", local_ip)

def shutdown_handler(unused_addr, args, value):
  print("OSC message : Shutting down system")
  os.system("shutdown -h -P now") 

dispatcher = dispatcher.Dispatcher()
dispatcher.map("/MS/shutdown", shutdown_handler, "Shutdown OSC")

server = osc_server.ThreadingOSCUDPServer((local_ip, 7702), dispatcher)
print("Serving on {}".format(server.server_address))

server.serve_forever()
