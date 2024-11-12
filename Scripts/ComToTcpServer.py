#
#  Script for piping a local serial connection to a TCP port.
#
#  Copyright (c) Microsoft Corporation.
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

import time
import sys
import win32pipe
import win32file
import pywintypes
import socket
import threading
import queue
import argparse
import serial

parser = argparse.ArgumentParser()
parser.add_argument("-l", "--logfile", type=str,
                    help="File path to log traffic")
parser.add_argument("-p", "--port", type=int, default=5555,
                    help="The TCP port to listen for connections.")
parser.add_argument("-n", "--pipe", type=str,
                    help="The named pipe to connect to connect to.")
parser.add_argument("-c", "--comport", type=str,
                    help="The number of the COM device to connect to. E.G 'COM5'")
parser.add_argument("-b", "--baudrate", type=int, default=115200,
                    help="The baudrate of the serial port.")
parser.add_argument("-v", "--verbose", action="store_true",
                    help="Enabled verbose prints.")
args = parser.parse_args()

# Global queues used between threads.
out_queue = queue.Queue()
in_queue = queue.Queue()
logfile = None
inout_last = None

def socket_thread():
    # Set up the socket.
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('', args.port))
    sock.listen()

    while True:
        print("Waiting for socket...")
        conn, addr = sock.accept()
        print(f"Socket Connected - {addr}")

        # clear the out queue before starting a connection
        while not out_queue.empty():
            out_queue.get()

        # use a short timeout to move on if no data is ready.
        conn.settimeout(0.01)

        # process the queue.
        connected = True
        while connected:
            try:
                while not out_queue.empty():
                    conn.sendall(out_queue.get())

                data = conn.recv(4096)
            except socket.timeout:
                data = None
            except Exception as e:
                print("Socket disconnected.")
                connected = False
                continue

            if data is not None and len(data) != 0:
                in_queue.put(data)

def write_log(inout, data):
    global inout_last
    global logfile

    if logfile is None:
        return

    if inout != inout_last:
        logfile.flush()
        if inout:
            logfile.write("\n\n<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n")
        else:
            logfile.write("\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n")
        inout_last = inout

    string = data.decode("ascii")
    logfile.write(string)

def listen_named_pipe():
    print("Waiting for pipe...")
    quit = False
    while not quit:
        try:
            handle = win32file.CreateFile(
                args.pipe,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0,
                None,
                win32file.OPEN_EXISTING,
                0,
                None
            )

            print("Pipe connected.")
            while True:
                if win32file.GetFileSize(handle) > 0:
                    hr, data = win32file.ReadFile(handle, 4096)
                    if (hr != 0):
                        print(f"Error reading: {hr}")
                        continue

                    write_log(False, data)
                    if args.verbose:
                        print(f"OUT: {data}")
                    out_queue.put(data)

                while not in_queue.empty():
                    data = in_queue.get()
                    write_log(True, data)
                    if args.verbose:
                        print(f"IN: {data}")
                    win32file.WriteFile(handle, data, None)

        except pywintypes.error as e:
            if e.args[0] == 2:
                if args.verbose:
                    print("No pipe yet, waiting...")
                time.sleep(1)
            elif e.args[0] == 109:
                print("broken pipe")
                quit = True

def listen_com_port():
    print("Opening com port...")
    serial_port = serial.Serial(args.comport, args.baudrate, parity=serial.PARITY_NONE,
                                stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS,
                                timeout=0.1)

    print(f"Opened {args.comport}.")
    while True:
        if serial_port.in_waiting > 0:
            data = serial_port.read(size=serial_port.in_waiting)
            write_log(False, data)
            if args.verbose:
                print(f"OUT: {data}")
            out_queue.put(data)

        while not in_queue.empty():
            data = in_queue.get()
            write_log(True, data)
            if args.verbose:
                print(f"IN: {data}")
            serial_port.write(data)

def main():
    if args.pipe is None and args.comport is None:
        print("Must specify a serial connection!")
        return

    global logfile
    if args.logfile is not None:
        logfile = open(args.logfile, 'w')
        logfile.write(f"arguments: {args} \n\n")

    # Create the thread for the TCP port.
    port_thread = threading.Thread(target=socket_thread)
    port_thread.daemon = True
    port_thread.start()

    try:
        if args.pipe is not None:
            listen_named_pipe()
        if args.comport is not None:
            listen_com_port()
        else:
            raise Exception("No serial port to connect to!")
    except:
        if logfile is not None:
            logfile.close()


main()
