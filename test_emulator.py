import subprocess
import threading
import time
import random
import sys

exe_name = "OS_Emu_Project.exe"  # or "./OS_Emu_Project" on Linux/Mac

proc = subprocess.Popen(
    [exe_name],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1
)

# Thread to print emulator output live
def read_output():
    for line in proc.stdout:
        print(line, end='')

output_thread = threading.Thread(target=read_output, daemon=True)
output_thread.start()

def send_cmd(command, delay=1):
    print(f">>> {command}")
    proc.stdin.write(command + '\n')
    proc.stdin.flush()
    time.sleep(delay)

try:
    send_cmd("initialize", delay=1)
    send_cmd("scheduler-start", delay=1)

    sleep_time = 5
    print(f"Waiting for {sleep_time} seconds...")
    time.sleep(sleep_time)

    send_cmd("scheduler-stop", delay=1)
    send_cmd("screen -ls", delay=1)

except Exception as e:
    print("Error:", e)
    proc.terminate()
    sys.exit(1)
