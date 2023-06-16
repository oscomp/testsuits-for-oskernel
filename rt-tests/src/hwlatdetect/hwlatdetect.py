#!/usr/bin/python

# (C) 2015 Clark Williams <williams@redhat.com>
# (C) 2009 Clark Williams <williams@redhat.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License Version 2
# as published by the Free Software Foundation.

from __future__ import print_function

import sys
import os
import time
import subprocess
import errno
import os.path

version = "0.6"
debugging = False
quiet = False
watch = False

def debug(str):
    if debugging: print(str)

def info(str):
    if not quiet: print(str)

#
# Class used to manage mounting and umounting the debugfs
# filesystem. Note that if an instance of this class mounts
# the debugfs, it will unmount when cleaning up, but if it
# discovers that debugfs is already mounted, it will leave
# it mounted.
#
class DebugFS(object):
    '''class to manage mounting/umounting the debugfs'''
    def __init__(self):
        self.premounted = False
        self.mounted = False
        self.mountpoint = ''
        f = open('/proc/mounts')
        for l in f:
            field = l.split()
            if field[2] == "debugfs":
                self.premounted = True
                self.mountpoint = field[1]
                break
        f.close()

    def mount(self, path='/sys/kernel/debug'):
        if self.premounted or self.mounted:
            debug("not mounting debugfs")
            return True
        debug("mounting debugfs at %s" % path)
        self.mountpoint = path
        cmd = ['/bin/mount', '-t', 'debugfs', 'none', path]
        self.mounted = (subprocess.call(cmd) == 0)
        if not self.mounted:
            raise RuntimeError("Failed to mount debugfs")
        return self.mounted

    def umount(self):
        if self.premounted or not self.mounted:
            debug("not umounting debugfs")
            return True
        debug("umounting debugfs")
        cmd = ['/bin/umount', self.mountpoint]
        self.mounted = not (subprocess.call(cmd) == 0)
        if self.mounted:
            raise RuntimeError("Failed to umount debugfs")
        return not self.mounted

    def getval(self, item, nonblocking=False):
        path = os.path.join(self.mountpoint, item)
        if nonblocking == False:
            f = open(path)
            val = f.readline()
            f.close()
        else:
            f = os.fdopen(os.open(path, os.O_RDONLY|os.O_NONBLOCK), "r")
            try:
                val = f.readline()
            except OSError as e:
                print ("errno: %s" % e)
                if e.errno == errno.EAGAIN:
                    val = None
                else:
                    raise
            except IOError as e:
                if e.errno == errno.EAGAIN:
                    val = None
                else:
                    raise
            f.close()
        return val

    def putval(self, item, value):
        path = os.path.join(self.mountpoint, item)
        f = open(path, "w")
        f.write(str(value))
        f.flush()
        f.close()

    def getpath(self, item):
        return os.path.join(self.mountpoint, item)

#
# Class used to manage loading and unloading of the
# hwlat kernel module. Like the debugfs class
# above, if the module is already loaded, this class will
# leave it alone when cleaning up.
#
class Kmod(object):
    ''' class to manage loading and unloading hwlat.ko'''

    names = ("hwlat_detector", "smi_detector")
    def __check_builtin(self):
        for l in open(os.path.join('/lib/modules', os.uname()[2], 'modules.builtin'), "r"):
            for m in Kmod.names:
                if m in l:
                    debug("found %s as builtin" % m)
                    return m
        return None

    def __find_modname(self):
        debug("looking for modules")
        path = os.path.join("/lib/modules",
                            os.uname()[2],
                            "kernel/drivers/misc")
        debug("module path: %s" % path)
        for m in Kmod.names:
            mpath = os.path.join(path, m) + ".ko"
            debug("checking %s" % mpath)
            if os.path.exists(mpath):
                return m
        raise RuntimeError("no detector module found!")

    def __init__(self):
        self.preloaded = False
        self.builtin = False

        # check for builtin
        self.modname = self.__check_builtin()
        if self.modname:
            self.builtin = True
            return

        # now look for module
        f = open ('/proc/modules')
        for l in f:
            field = l.split()
            if field[0] in Kmod.names:
                self.preloaded = True
                self.modname = field[0]
                debug("using already loaded %s" % self.modname)
                f.close()
                break
        f.close()
        self.modname = self.__find_modname()

    def load(self):
        if self.builtin:
            debug("not loading %s (builtin)" % self.modname)
            return True
        if self.preloaded:
            debug("not loading %s (already loaded)" % self.modname)
            return True
        cmd = ['/sbin/modprobe', self.modname]
        return (subprocess.call(cmd) == 0)

    def unload(self):
        if self.preloaded or self.builtin:
            debug("Not unloading %s" % self.modname)
            return True
        cmd = ['/sbin/modprobe', '-r', self.modname]
        return (subprocess.call(cmd) == 0)

#
# wrapper class for detection modules
#
class Detector(object):
    '''wrapper class for managing detector modules'''
    def __init__(self):
        if os.getuid() != 0:
            raise RuntimeError("Must be root")
        self.debugfs = DebugFS()
        self.kmod = Kmod()
        self.setup()
        if self.kmod.modname == "hwlat_detector":
            self.detector = Hwlat(self.debugfs)
        elif self.kmod.modname == "smi_detector":
            self.detector = Smi(self.debugfs)
        self.samples = []
        self.testduration = 10 # ten seconds
        self.have_msr = False
        self.initsmi = []
        if os.path.exists('/usr/sbin/rdmsr'):
            self.have_msr = True
            self.initsmi = self.getsmicounts()

    def getsmicounts(self):
        counts = []
        if self.have_msr:
            p = subprocess.Popen(['/usr/sbin/rdmsr', '-a', '-d', '0x34'], stdout=subprocess.PIPE)
            p.wait()
            counts = [ int(x.strip()) for x in p.stdout.readlines()]
        return counts

    def force_cleanup(self):
        debug("forcing unload of hwlat module")
        self.kmod.preloaded = False
        debug("forcing unmount of debugfs")
        self.debugfs.premounted = False
        self.cleanup()
        debug("exiting")
        sys.exit(0)

    def setup(self):
        if not self.debugfs.mount():
            raise RuntimeError("Failed to mount debugfs")
        if not self.kmod.load():
            raise RuntimeError("Failed to unload hwlat")

    def cleanup(self):
        if not self.kmod.unload():
            raise RuntimeError("Failed to unload hwlat")
        if not self.debugfs.umount():
            raise RuntimeError("Failed to unmount debugfs")

    def get(self, field):
        return self.detector.get(field)

    def set(self, field, val):
        return self.detector.set(field, val)

    def start(self):
        count = 0
        threshold = self.get("threshold")
        debug("enabling detector module (threshold: %d)" % threshold)
        self.detector.set("enable", 1)
        debug("first attempt at enable")
        while self.detector.get("enable") == 0:
            debug("still disabled, retrying in a bit")
            count += 1
            time.sleep(0.1)
            debug("retrying enable of detector module (%d)" % count)
            self.detector.set("enable", 1)
        if self.get("threshold") != threshold:
            debug("start: threshold reset by start, fixing")
            self.set("threshold", threshold)
        debug("detector module enabled (threshold: %d)" % self.get("threshold"))

    def stop(self):
        count = 0
        debug("disabling detector module")
        self.detector.set("enable", 0)
        debug("first attempt at disable");
        while self.detector.get("enable") == 1:
            debug("still enabled, retrying in a bit")
            count += 1
            time.sleep(0.1)
            debug("retrying disable of detector module(%d)" % count)
            self.detector.set("enable", 0)
        debug("detector module disabled")

    def detect(self):
        debug("Starting hardware latency detection for %d seconds" % self.testduration)
        self.start()
        try:
            self.samples = self.detector.detect(self.testduration)
        finally:
            self.stop()
        debug("Hardware latency detection done (%d samples)" % len(self.samples))
#
# Class to simplify running the hwlat kernel module
#
class Hwlat(object):
    '''class to wrap access to hwlat debugfs files'''
    def __init__(self, debugfs):
        self.debugfs = debugfs

    def get(self, field):
        return int(self.debugfs.getval(os.path.join("hwlat_detector", field)))

    def set(self, field, val):
        if field == "enable" and val:
            val = 1
        self.debugfs.putval(os.path.join("hwlat_detector", field), str(val))

    def get_sample(self):
        return self.debugfs.getval("hwlat_detector/sample", nonblocking=True)

    def detect(self, duration):
        self.samples = []
        testend = time.time() + duration
        pollcnt = 0
        try:
            while time.time() < testend:
                pollcnt += 1
                val = self.get_sample()
                while val:
                    val = val.strip()
                    self.samples.append(val)
                    if watch: print(val)
                    debug("got a latency sample: %s" % val)
                    val = self.get_sample()
                time.sleep(0.1)
        except KeyboardInterrupt as e:
            print("interrupted")
            sys.exit(1)
        return self.samples
#
# the old smi_detector.ko module has different debugfs entries than the modern
# hwlat_detector.ko module; this object translates the current entries into the
# old style ones. The only real issue is that the smi_detector module doesn't
# have the notion of width/window, it has the sample time and the interval
# between samples. Of course window == sample time + interval, but you have to
# have them both to calculate the window.
#

class Smi(object):
    '''class to wrap access to smi_detector debugfs files'''
    field_translate = {
        "count" : "smi_count",
        "enable" : "enable",
        "max" : "max_sample_us",
        "sample" : "sample_us",
        "threshold" : "latency_threshold_us",
        "width" : "ms_per_sample",
        "window" : "ms_between_sample",
        }

    def __init__(self, debugfs):
        self.width = 0
        self.window = 0
        self.debugfs = debugfs

    def __get(self, field):
        return int(self.debugfs.getval(os.path.join("smi_detector", field)))

    def __set(self, field, value):
        debug("__set: %s <-- %d" % (field, value))
        self.debugfs.putval(os.path.join("smi_detector", field), str(value))
        if self.__get(field) != value:
            raise RuntimeError("Error setting %s to %d (%d)" % (field, value, self.__get(field)))

    def get(self, field):
        name = Smi.field_translate[field]
        if name != field:
            debug("get: %s translated to %s" % (field, name))
        if field == "window":
            return self.get_window()
        elif field == "width":
            return ms2us(self.__get(name))
        else:
            return self.__get(name)

    def get_window(self):
        sample = ms2us(self.__get('ms_per_sample'))
        interval = ms2us(self.__get('ms_between_samples'))
        return sample + interval

    def set_window(self, window):
        width = ms2us(int(self.__get('ms_per_sample')))
        interval = window - width
        if interval <= 0:
            raise RuntimeError("Smi: invalid width/interval values (%d/%d (%d))" % (width, interval, window))
        self.__set('ms_between_samples', us2ms(interval))

    def set(self, field, val):
        name = Smi.field_translate[field]
        if name != field:
            debug ("set: %s translated to %s" % (field, name))
        if field == "enable" and val:
            val = 1
        if field == "window":
            self.set_window(val)
        else:
            if field == "width":
                val = us2ms(val)
            self.__set(name, val)

    def get_sample(self):
        name = Smi.field_translate["sample"]
        return self.debugfs.getval(os.path.join('smi_detector', name), nonblocking=True)

    def detect(self, duration):
        self.samples = []
        testend = time.time() + duration
        threshold = self.get("threshold")
        debug("detect: threshold %d" % threshold)
        pollcnt = 0
        try:
            while time.time() < testend:
                pollcnt += 1
                val = self.get_sample()
                val = val.strip()
                if int(val) >= threshold:
                    self.samples.append(val)
                    if watch: print(val)
                    debug("got a latency sample: %s (threshold: %d)" % (val, self.get("threshold")))
                time.sleep(0.1)
        except KeyboardInterrupt as e:
            print("interrupted")
            sys.exit(1)
        return self.samples


def ms2us(val):
    return val * 1000

def us2ms(val):
    return val / 1000

def seconds(str):
    "convert input string to value in seconds"
    if str.isdigit():
        return int(str)
    elif str[-2].isalpha():
        raise RuntimeError("illegal suffix for seconds: '%s'" % str[-2:-1])
    elif str[-1:] == 's':
        return int(str[0:-1])
    elif str[-1:] == 'm':
        return int(str[0:-1]) * 60
    elif str[-1:] == 'h':
        return int(str[0:-1]) * 3600
    elif str[-1:] == 'd':
        return int(str[0:-1]) * 86400
    elif str[-1:] == 'w':
        return int(str[0:-1]) * 86400 * 7
    else:
        raise RuntimeError("invalid input for seconds: '%s'" % str)

def milliseconds(str):
    "convert input string to millsecond value"
    if str.isdigit():
        return int(str)
    elif str[-2:] == 'ms':
        return int(str[0:-2])
    elif str[-1] == 's':
        return int(str[0:-2]) * 1000
    elif str[-1] == 'm':
        return int(str[0:-1]) * 1000 * 60
    elif str[-1] == 'h':
        return int(str[0:-1]) * 1000 * 60 * 60
    else:
        raise RuntimeError("invalid input for milliseconds: %s" % str)


def microseconds(str):
    "convert input string to microsecond value"
    if str.isdigit():
        return int(str)
    elif str[-2:] == 'ms':
        return (int(str[0:-2]) * 1000)
    elif str[-2:] == 'us':
        return int(str[0:-2])
    elif str[-1:] == 's':
        return (int(str[0:-1]) * 1000 * 1000)
    else:
        raise RuntimeError("invalid input for microseconds: '%s'" % str)

if __name__ == '__main__':
    from optparse import OptionParser

    parser = OptionParser()
    parser.add_option("--duration", default=None, type="string",
                      dest="duration",
                      help="total time to test for hardware latency (<n>{smdw})")

    parser.add_option("--threshold", default=None, type="string",
                      dest="threshold",
                      help="value above which is considered an hardware latency")

    parser.add_option("--window", default=None, type="string",
                      dest="window",
                      help="time between samples")

    parser.add_option("--width", default=None, type="string",
                      dest="width",
                      help="time to actually measure")

    parser.add_option("--report", default=None, type="string",
                      dest="report",
                      help="filename for sample data")

    parser.add_option("--cleanup", action="store_true", default=False,
                      dest="cleanup",
                      help="force unload of module and umount of debugfs")

    parser.add_option("--debug", action="store_true", default=False,
                      dest="debug",
                      help="turn on debugging prints")

    parser.add_option("--quiet", action="store_true", default=False,
                      dest="quiet",
                      help="turn off all screen output")

    parser.add_option("--watch", action="store_true", default=False,
                      dest="watch",
                      help="print sample data to stdout as it arrives")

    (o, a) = parser.parse_args()

    # need these before creating detector instance
    if o.debug:
        debugging = True
        quiet = False
        debug("debugging prints turned on")

    if o.quiet:
        quiet = True
        debugging = False

    detect = Detector()

    if o.cleanup:
        debug("forcing cleanup of debugfs and hardware latency module")
        detect.force_cleanup()
        sys.exit(0)

    if o.threshold:
        t = microseconds(o.threshold)
        detect.set("threshold", t)
        debug("threshold set to %dus" % t)

    if o.window:
        w = microseconds(o.window)
        if w < detect.get("width"):
            debug("shrinking width to %d for new window of %d" % (w/2, w))
            detect.set("width", w/2)
        debug("window parameter = %d" % w)
        detect.set("window", w)
        debug("window for sampling set to %dus" % w)

    if o.width:
        w = microseconds(o.width)
        if w > detect.get("window"):
            debug("widening window to %d for new width of %d" % (w*2, w))
            detect.set("window", w*2)
        debug("width parameter = %d" % w)
        detect.set("width", w)
        debug("sample width set to %dus" % w)

    if o.duration:
        detect.testduration = seconds(o.duration)
    else:
        detect.testduration = 120 # 2 minutes
    debug("test duration is %ds" % detect.testduration)

    if o.watch:
        watch = True

    reportfile = o.report

    info("hwlatdetect:  test duration %d seconds" % detect.testduration)
    info("   parameters:")
    info("        Latency threshold: %dus" % detect.get("threshold"))
    info("        Sample window:     %dus" % detect.get("window"))
    info("        Sample width:      %dus" % detect.get("width"))
    info("     Non-sampling period:  %dus" % (detect.get("window") - detect.get("width")))
    info("        Output File:       %s" % reportfile)
    info("\nStarting test")

    detect.detect()

    info("test finished")

    exceeding = detect.get("count")
    info("Max Latency: %dus" % detect.get("max"))
    info("Samples recorded: %d" % len(detect.samples))
    info("Samples exceeding threshold: %d" % exceeding)

    if detect.have_msr:
        finishsmi = detect.getsmicounts()
        total_smis = 0
        for i,count in enumerate(finishsmi):
            if count > detect.initsmi[i]:
                smis = count - detect.initsmi[i]
                total_smis += smis
                print("%d SMIs occured on cpu %d" % (smis, i))
        info("SMIs during run: %d" % total_smis)

    if reportfile:
        count = 0
        f = open(reportfile, "w")
        for s in detect.samples:
            count += 1
            f.write("%s\n" % s)
        f.close()
        info("sample data (%d samples) written to %s" % (count, reportfile))
    else:
        for s in detect.samples:
            print("%s" % s)

    detect.cleanup()
    sys.exit(exceeding)
