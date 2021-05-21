import serial
import time
import subprocess
import sys
import os

result = {}


def prepare_sdcard(usb_port):
    port_dir = arg.split('/')[-1]

    flash_tool_path = f"/cg/{port_dir}/ktool.py"
    image_sdcard_write_output_path = f"/cg/{port_dir}/sdcard_write.txt"
    image_flash_output_path = f"/cg/{port_dir}/sdcard_flash.txt"
    image_path = f"/cg/{port_dir}/img.kfpkg"

    flash_sdcard_cmd = f"python3 {flash_tool_path} -b 3000000 -B dan {image_path} -p {usb_port} > {image_flash_output_path}"

    print(flash_sdcard_cmd, file=sys.stderr)

    for i in range(3):
        process = subprocess.Popen(flash_sdcard_cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   encoding="utf8")
        out, err = process.communicate()
        # flash_sdcard_cmd = "kflash -b 3000000 -B dan -p /dev/ttyUSB0 " + image_path + " > " + image_flash_output_path
        # with open(image_flash_output_path, "w") as f:
        #     f.write(flash_sdcard_cmd + "\n")
        with open(image_flash_output_path) as f:
            data = f.read()
            if "[ERROR]" in data:
                if i == 2:
                    raise RuntimeError("烧写SD卡重试次数超过3次")
                continue
            else:
                break

    result['sdcard_run_out'] = out
    result['sdcard_run_stderr'] = err

    ss = serial.Serial(usb_port, 115200,
                       timeout=5,
                       parity=serial.PARITY_NONE,
                       stopbits=serial.STOPBITS_ONE,
                       bytesize=serial.EIGHTBITS)

    if not ss.isOpen():
        raise RuntimeError("OPEN Serial Fail when Flash Sdcard")

    outf = open(image_sdcard_write_output_path, "w+")
    start_time = time.time()
    ss.setDTR(False)
    ss.setRTS(True)
    ss.setRTS(False)
    while True:
        cur_time = time.time()
        use_time = cur_time - start_time
        if use_time > 180:
            outf.write("Flash time too long")
            raise RuntimeError("Flash time too long")
        data = ss.readline()
        try:
            data = data.decode()
        except UnicodeDecodeError:
            data = str(data)
        outf.write(str(data))
        outf.flush()
        if 'finish' in data:
            outf.write("\nFINISH\n")
            break

    outf.close()
    ss.close()


def ssh_run(usb_port):
    port_dir = arg.split('/')[-1]

    flash_cmd = f"kflash -b 3000000 -B dan /cg/{port_dir}/k210.bin -p {usb_port} > /cg/{port_dir}/k210_flash_out.txt"
    print(flash_cmd, file=sys.stderr)
    # with open(f"/cg/{port_dir}/k210_flash_out.txt", "w") as f:
    #     f.write(flash_cmd + '\n')
    # print(flash_cmd)

    outf = open(f"/cg/{port_dir}/k210_serial_out.txt", "w+")

    for i in range(3):
        process = subprocess.Popen(flash_cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                   encoding="utf8")
        out, err = process.communicate()
        with open(f"/cg/{port_dir}/k210_flash_out.txt") as f:
            data = f.read()
            if "[ERROR]" in data:
                if i == 2:
                    raise RuntimeError("烧写固件重试超过3次")
                continue
            else:
                break

    result['ssh_run_out'] = out
    result['ssh_run_stderr'] = err

    # ---RUN---
    # open serial
    ss = serial.Serial(usb_port, 115200,
                       timeout=2,
                       parity=serial.PARITY_NONE,
                       stopbits=serial.STOPBITS_ONE,
                       bytesize=serial.EIGHTBITS)
    if not ss.isOpen():
        result['verdict'] = "SerialError"
        result['stderr'] = "OPEN Serial fail"
        result['return_code'] = 1
        return result
    #loge("\n[k210 autotest]:" + str(ss) + "\n")

    #loge("\n[k210 autotest]: Rebooting\n")

    # # read from serial
    #loge("\n[k210 autotest]: Read from serial\n")
    # time_start  = time.time()
    ss.setDTR(False)
    ss.setRTS(True)
    ss.setRTS(False)
    # time.sleep(0.1)
    while (True):
        # cur_time = time.time()
        # use_time = cur_time - time_start
        data = ss.readline()
        try:
            data = data.decode()
        except UnicodeDecodeError:
            data = str(data)
        # #loge("[k210 autotest]"+str(use_time) + ' ss:',end =' ')
        # loge(str(data), end = '')
        if len(data) == 0:
            break
        outf.write(str(data))
        outf.flush()
    outf.close()
    ss.close()
    return result


if __name__ == '__main__':
    arg = sys.argv[1]
    port = arg.split('/')[-1]
    # os.system(f"rm /cg/{port}/*")
    # if arg == 'clean':
    #     os.system("rm /cg/*.bin")
    #     os.system("rm /cg/*.txt")
    # else:
    prepare_sdcard(arg)
    print(ssh_run(arg))



