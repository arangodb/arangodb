#!/usr/bin/env python

''' The module for runtime performance monitoring.'''

license = \
'''
 Copyright (c) 2009, Kluchnikov Ivan <kluchnikoviATgmailDOTcom> 
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
 3. The names of the authors can not be used to endorse or promote
    products derived from this software without specific prior written
    permission.

 THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS
 OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
'''

import subprocess
import threading
import datetime
import numpy
import sys
import os


if sys.platform == 'win32':
  import win32pdh

if sys.platform == 'linux2':
  namesStat = [("pid", numpy.int_),
               ("comm", 'S256'),
               ("state", 'S1'),
               ("ppid", numpy.int_),
               ("pgrp", numpy.int_),
               ("session", numpy.int_),
               ("tty_nr", numpy.int_),
               ("tpgid", numpy.int_),
               ("flags", numpy.uint),
               ("minflt", numpy.ulonglong),
               ("cminflt", numpy.ulonglong),
               ("majflt", numpy.ulonglong),
               ("cmajflt", numpy.ulonglong),
               ("utime", numpy.ulonglong),
               ("stime", numpy.ulonglong),
               ("cutime", numpy.longlong),
               ("cstime", numpy.longlong),
               ("priority", numpy.longlong),
               ("nice", numpy.longlong),
               ("num_threads", numpy.longlong),
               ("itrealvalue", numpy.longlong),
               ("starttime", numpy.ulonglong),
               ("vsize", numpy.ulonglong),
               ("rss", numpy.longlong),
               ("rsslim", numpy.ulonglong),
               ("startcode", numpy.ulonglong),
               ("endcode", numpy.ulonglong),
               ("startstack", numpy.ulonglong),
               ("kstkesp", numpy.ulonglong),
               ("kstkeip", numpy.ulonglong),
               ("signal", numpy.ulonglong),
               ("blocked", numpy.ulonglong),
               ("sigignore", numpy.ulonglong),
               ("sigcatch", numpy.ulonglong),
               ("wchan", numpy.ulonglong),
               ("nswap", numpy.ulonglong),
               ("cnswap", numpy.ulonglong),
               ("exit_signal", numpy.int_),
               ("processor", numpy.int_),
               ("rt_priority", numpy.uint),
               ("policy", numpy.uint),
               ("delayacct_blkio_ticks", numpy.ulonglong),
               ("guest_time", numpy.ulonglong),
               ("cguest_time", numpy.longlong)]

  namesStatm = [("size", numpy.int_),
                ("resident", numpy.int_),
                ("share", numpy.int_),
                ("trs", numpy.int_),
                ("lrs", numpy.int_),
                ("drs", numpy.int_),
                ("dt", numpy.int_)]

if sys.platform == 'win32':
  procItemsTypes = [[numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.long, win32pdh.PDH_FMT_LONG],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                    [numpy.double, win32pdh.PDH_FMT_DOUBLE]]
  
  threadItemsTypes =[[numpy.double, win32pdh.PDH_FMT_DOUBLE],
                     [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                     [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                     [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                     [numpy.double, win32pdh.PDH_FMT_DOUBLE],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG],
                     [numpy.long, win32pdh.PDH_FMT_LONG]]  
 
  strProcess = "Process"
  strThread = "Thread"
  namesProcess = []
  namesThread = []
  iType = 0
  
  procItems, procInsts = win32pdh.EnumObjectItems(None, None, strProcess, win32pdh.PERF_DETAIL_WIZARD)
  for procItem in procItems: 
    namesProcess.append((procItem, procItemsTypes[iType][0]))
    iType = iType + 1
  iType = 0
  threadItems, threadInsts = win32pdh.EnumObjectItems(None, None, strThread, win32pdh.PERF_DETAIL_WIZARD)
  for threadItem in threadItems: 
    namesThread.append((threadItem, threadItemsTypes[iType][0]))
    iType = iType + 1
 
procNames = []
threadNames = []
namesP = []
namesT = []
formatsP = []
formatsT = []
   
procNames.append(("time", numpy.ulonglong))
if sys.platform == 'linux2':
  procNames.extend(namesStat)
  procNames.extend(namesStatm) 
if sys.platform == 'win32':
  procNames.extend(namesProcess)

threadNames.append(("time", numpy.ulonglong))
if sys.platform == 'linux2':
  threadNames.extend(namesStat)
if sys.platform == 'win32':
  threadNames.extend(namesThread)
   
for procName in procNames:
  namesP.append(procName[0])
  formatsP.append(procName[1])
   
for threadName in threadNames:
  namesT.append(threadName[0])
  formatsT.append(threadName[1])

typeProc = numpy.dtype({'names':namesP, 'formats':formatsP}) 
typeThread = numpy.dtype({'names':namesT, 'formats':formatsT}) 

def getFullMemSize():
  """
  int = getFullMemSize()
  Returns full memory size in bytes (linux) 
  """
  f = open("/proc/meminfo")
  mem = f.readline()
  f.close()
  return 1024 * int(mem[10:-3].strip())

if sys.platform == 'linux2':
  fullmemsize = getFullMemSize()
  HZ = os.sysconf('SC_CLK_TCK')

def getTime(key):
  """
  long = getTime(key)
  Returns the time in microseconds
  key: datetime object       
  """
  return (((key.weekday() * 24 + key.hour) * 60 + key.minute) * 60 + key.second) * 1000000 + key.microsecond
 
def findProcess(pid):
  """
  instanceTuple = findProcess(pid)
  Finds process with necessary pid in all processes (windows)
  pid: int
       process ID
  instanceTuple: tuple
         (instanceIndex, instanceName)
         instanceIndex: int or None
              process instance index
         instanceName: string or None
              process instance name
  """
  items, procInsts = win32pdh.EnumObjectItems(None, None, strProcess, win32pdh.PERF_DETAIL_WIZARD)
  for procInst in procInsts:
    hQuery = win32pdh.OpenQuery()
    index = 0
    while index != -1:
      path = win32pdh.MakeCounterPath((None, strProcess, procInst, None,index, namesP[15]))
      if win32pdh.ValidatePath(path) == 0:
        hCounter = win32pdh.AddCounter(hQuery, path)
        win32pdh.CollectQueryData(hQuery)
        type, val = win32pdh.GetFormattedCounterValue(hCounter, win32pdh.PDH_FMT_LONG)
        if val == pid:
          win32pdh.RemoveCounter(hCounter)
          win32pdh.CloseQuery(hQuery)
          return index, procInst
        else:
          index = index + 1 
      else:
        index = -1 
  win32pdh.RemoveCounter(hCounter)
  win32pdh.CloseQuery(hQuery)  
  return None, None

def findThreads(pid, inst): 
  """
  threadsList = findThreads(pid, inst)
  Finds threads with necessary pid in all threads (windows)
  pid: int
       process ID
  inst: string
       part of thread instance name
  threadsList: list
       consist of tuples (instanceName, instanceIndex)
       instanceIndex: int
            thread instance index
       instanceName: string
            thread instance name
  """
  threads = []
  index = -1
  while index != -2:
    i = 0
    if index != 0:
      while i != -1:
        threadInst = inst + '/' + str(i)
        hQuery = win32pdh.OpenQuery()
        path = win32pdh.MakeCounterPath((None, strThread, threadInst, None, index,namesT[11]))
        if win32pdh.ValidatePath(path) == 0:
          hCounter = win32pdh.AddCounter(hQuery, path)
          win32pdh.CollectQueryData(hQuery)
          type, val = win32pdh.GetFormattedCounterValue(hCounter, win32pdh.PDH_FMT_LONG)
          if val == pid:
            threads.append((threadInst, index))
            i = i + 1
          else:
            if index==-1:
              i = i + 1
            else:
              index = index + 1
              break
        else:
          if i == 0:
            i = 0
            index = -2
            break
          else:
            i = 0
            index = index + 1
            break
        win32pdh.RemoveCounter(hCounter)
        win32pdh.CloseQuery(hQuery)
    else:
      index = index + 1
  return threads

def collectProcessData(pid):
  """
  array = collectProcessData(pid)
  Collects data for given process (linux)
  pid: int
       process ID
  array: numpy array
       process data array (dtype = typeProc)   
  """
  data = [] 
  f1 = open("/proc/%d/stat"%(pid, ))
  f2 = open("/proc/%d/statm"%(pid, ))
  
  stat = f1.readline().split()
  mem = f2.readline().split()
  data = tuple([getTime(datetime.datetime.now())] + stat + mem)
  res = numpy.array([data], dtype = typeProc)
  return res

def createProcessCountersHandles(index, inst):
  """
  procHCounters, procHQueries = createProcessCountersHandles(index, inst)
  Creates handels for process counters and handels for process queries.
  index: int
       process instance index
  inst: string
       process instance name
  procHCounters: array
       handels for process counters
  procHQueries: array
       handels for process queries
  """
  procHQueries = []
  procHCounters = []
  for item in procItems:
      procHQueries.append(win32pdh.OpenQuery())
      path = win32pdh.MakeCounterPath((None, strProcess, inst, None, index, item))
      try:
        procHCounters.append(win32pdh.AddCounter(procHQueries[-1], path))
      except:
        return [], []
  i = 0
  length = len(procHCounters)
  for ln in range(length):
    try:
      win32pdh.CollectQueryData(procHQueries[ln])
      type, val = win32pdh.GetFormattedCounterValue(procHCounters[ln], procItemsTypes[i][1])
      i = i + 1
    except:
      return [], []
  return procHCounters, procHQueries

def collectProcessDataWin(index, inst, procHCounters, procHQueries):
  """
  array = collectProcessData(index, inst, procHCounters, procHQueries)
  Collects data for given process (windows)
  index: int
       process instance index
  inst: string
       process instance name
  procHCounters: array
       handels for process counters
  procHQueries: array
       handels for process queries
  array: numpy array
       process data array (dtype = typeProc)
  """
  data = [] 
  stat = []
  i = 0
  length = len(procHCounters)
  if length == 0:
    return None
  for ln in range(length):
    try:
      win32pdh.CollectQueryData(procHQueries[ln])
      type, val = win32pdh.GetFormattedCounterValue(procHCounters[ln], procItemsTypes[i][1])
      stat.append(val)
      i = i + 1
    except:
      return None 
  data = tuple([getTime(datetime.datetime.now())] + stat)
  res = numpy.array([data], dtype = typeProc) 
  return res

def collectThreadData(pid, task):
  """
  array = collectThreadData(pid, task)
  Collects data for given thread (linux)
  pid: int
       process ID
  task: string
       thread ID
  array: numpy array
       thread data array (dtype = typeThread)   
  """
  data = []
  f1 = open("/proc/%d/task/%s/stat"%(pid, task))
  stat = f1.readline().split()
  data = tuple([getTime(datetime.datetime.now())] + stat)
  res = numpy.array([data], dtype = typeThread) 
  return res

def createThreadCountersHandles(thread):
  """
  threadHCounters, threadHQueries = createThreadCountersHandles(thread)
  Creates handels for thread's counters and handels for thread's queries.
  thread: tuple
       (instanceName, instanceIndex)
        instanceIndex: int
             thread instance index
        instanceName: string
             thread instance name
  threadHCounters: array
       handels for thread's counters
  threadHQueries: array
       handels for thread's queries
  """
  threadHQueries = []
  threadHCounters = []
  for item in threadItems:
      threadHQueries.append(win32pdh.OpenQuery())
      path = win32pdh.MakeCounterPath((None, strThread, thread[0], None, thread[1], item))
      try:
        threadHCounters.append(win32pdh.AddCounter(threadHQueries[-1], path))
      except:
        return [], []
  i = 0
  length = len(threadHCounters)
  for ln in range(length):
    try:
      win32pdh.CollectQueryData(threadHQueries[ln])
      type, val = win32pdh.GetFormattedCounterValue(threadHCounters[ln], threadItemsTypes[i][1])
      i = i + 1
    except:
      return [], []
  return threadHCounters, threadHQueries  

def collectThreadDataWin(thread, threadHCounters, threadHQueries):
  """
  array = collectThreadData(thread)
  Collects data for given thread (windows)
  thread: tuple
       (instanceName, instanceIndex)
        instanceIndex: int
             thread instance index
        instanceName: string
             thread instance name
  threadHCounters: array
       handels for thread's counters
  threadHQueries: array
       handels for thread's queries
  array: numpy array or None
       thread data array (dtype = typeThread)   
  """
  data = [] 
  stat = []
  i = 0
  length = len(threadHCounters)
  if length == 0:
    return None
  for ln in range(length):
    try:
      win32pdh.CollectQueryData(threadHQueries[ln])
      type, val = win32pdh.GetFormattedCounterValue(threadHCounters[ln], threadItemsTypes[i][1])
      stat.append(val)
      i = i + 1
    except:
      return None 
  data = tuple([getTime(datetime.datetime.now())] + stat)
  res = numpy.array([data], dtype = typeThread) 
  return res

def getCPU(data, previousData):
    """
    list =  getCPU(data, previousData)
    Calculates CPU usage values (linux)
    data: numpy array
         data array (dtype = typeThread or dtype = typeProc)
    previousData: numpy array
         data array (dtype = typeThread or dtype = typeProc) with length = 1
    list: list
         list of CPU usage values
    """
    result = []    
    for dat in data:
      if previousData != None:
       time_delta = dat['time'] - previousData['time']
       jiffies_delta = (dat['utime'] + dat['stime']) - (previousData['utime'] + previousData['stime'])
       result.append (100 * (float(jiffies_delta) / HZ) / (float(time_delta) / 1e6))
      else:
       result.append(0)
       previousData = dat
    return result

def parseData (data, query, namesData, previousData):
    """
    array = parseData (data, query, namesData, previousData)
    Parses given data using query
    data: numpy array
         data array (dtype = typeThread or dtype = typeProc)
    query: list
         name list of process or thread counters
    namesData: list
         procNames or threadNames 
    previousData: numpy array
         data array (dtype = typeThread or dtype = typeProc) with length = 1    
    array: numpy array
         data array 
    """
    names = []
    formats = []
    temp = []
    for qr in query:
      if qr == 'cpuUsage':
        if sys.platform == 'linux2':
          cpu = getCPU(data, previousData)
        names.append(qr)
        formats.append(numpy.float)
      elif qr == 'memUsage':
        if len(namesData) > len(threadNames):
          if sys.platform == 'linux2':
            mem = []
            for dat in data:
              mem.append(float(dat['vsize']) / fullmemsize)        
          names.append(qr)
          formats.append(numpy.float)
      else:
        for nameData in namesData:
          if qr == nameData[0]:
            names.append(nameData[0])
            formats.append(nameData[1])
    for name in names:
      if name == 'cpuUsage':
        if sys.platform == 'linux2':
          temp.append(cpu)
        elif sys.platform == 'win32':
          temp.append(data[namesP[1]])
      elif name == 'memUsage':
        if sys.platform == 'linux2':
          temp.append(mem)
        elif sys.platform == 'win32':
          temp.append(data[namesP[11]])
      else:
        temp.append(data[name]) 
    temp = numpy.transpose(temp)
    i = 0
    list = []    
    while i != len(temp):
      list.append(tuple(temp[i]))
      i = i + 1
    type=numpy.dtype({'names':names, 'formats':formats}) 
    data = numpy.array(list, dtype = type )    
    return data 

def parseQuery (dataProcess, dataThreads, procQuery, threadsQuery, prevProcData, prevThreadsData):
    """
    data = parseQuery (dataProcess, dataThreads, procQuery, threadsQuery, prevProcData, prevThreadsData) 
    Parses given query
    dataProcess: numpy array
         process data array (dtype = typeProc)
    dataThreads: dictionary
         threads data dictionary consists of thread data numpy arrays (dtype = typeThread)
    procQuery: list
         name list of process counters
    threadsQuery: list
         name list of thread counters
    prevProcData: numpy array
         data array (dtype = typeProc) with length = 1
    prevThreadsData: dictionary
         threads data dictionary consists of thread data numpy arrays (dtype = typeThread) with length = 1
    data: tuple 
         (dataProcess, dataThreads) 
    """
    if procQuery != None:
      dataProcess = parseData (dataProcess, procQuery, procNames, prevProcData)
    if threadsQuery != None:
      data = {}
      threads = dataThreads.keys()
      for thread in threads:
        prevThreadData = None
        if prevThreadsData != None:
          if prevThreadsData.has_key(thread) == 1:
            prevThreadData = prevThreadsData[thread]
        data[thread] = parseData (dataThreads[thread], threadsQuery, threadNames, prevThreadData)
      dataThreads = data
    return dataProcess, dataThreads
        
class MonitorThread(threading.Thread):
  """
  MonitorThread is descendant of threading.Thread class.
  This class represents a thread of control and can be safely subclassed in a limited fashion.
  By default MonitorThread saves the process info every 0.5 seconds,
  but you can set necessary updateInterval (in seconds). 
  """
  def __init__(self, pid, updateInterval = 0.5):
    self.pid = pid
    threading.Thread.__init__(self)
    self.dataProcess = numpy.array([], dtype = typeProc)
    self.dataThreads = {}
    self.process = True
    self.updateInterval = updateInterval
    self.lock = threading.RLock()
    self.function = None
    self.collect = False
    self.getDataProcess = numpy.array([], dtype = typeProc)
    self.getDataThreads = {}
    self.lastThreads = []
    self.callbackProcQuery = None
    self.callbackThreadsQuery = None
    self.callbackPrevProcData = None
    self.callbackPrevThreadsData = None
    self.getdataPrevProcData = None
    self.getdataPrevThreadsData = None
    self.procHQueries = []
    self.procHCounters = []
    self.threadHandles = {}
  
  def run(self):
    """
    Method representing the thread's activity.
    """
    import os
    import time
    
    process = True
   
    if sys.platform == 'win32':
      pathIndex = None
      pathInst = None
      
    while process:
      self.lock.acquire()   
      if self.collect == True or self.function != None:

       if sys.platform == 'linux2':
         self.dataProcess = numpy.append(self.dataProcess, collectProcessData(self.pid))
         threads = os.listdir("/proc/%d/task/" % self.pid)
         for thread in threads:
           if self.dataThreads.has_key(thread) == 1:
             self.dataThreads[thread] = numpy.append(self.dataThreads[thread], collectThreadData(self.pid, thread))
           else:
             self.dataThreads[thread] = collectThreadData(self.pid, thread)
         self.lastThreads = threads
       
       if sys.platform == 'win32':
         if pathIndex == None and pathInst == None:
           pathIndex, pathInst = findProcess(self.pid)
         if len(self.procHCounters) == 0:
           self.procHCounters, self.procHQueries = createProcessCountersHandles(pathIndex, pathInst)
         procData = collectProcessDataWin(pathIndex, pathInst, self.procHCounters, self.procHQueries)         
         if procData != None:
           self.dataProcess = numpy.append(self.dataProcess, procData)
         threads = findThreads(self.pid, pathInst)
         tids = []
         for thread in threads:
           if self.threadHandles.has_key(thread) != 1:
             self.threadHandles[thread] = createThreadCountersHandles(thread)
         for thread in threads:
           data = collectThreadDataWin(thread, self.threadHandles[thread][0], self.threadHandles[thread][1])
           if data != None:
             tid = int(data[namesT[12]][0])
             tids.append(tid)
             if self.dataThreads.has_key(tid) == 1:
               self.dataThreads[tid] = numpy.append(self.dataThreads[tid], data)
             else:
               self.dataThreads[tid] = data
         self.lastThreads = tids
       
       if self.function != None:
         dataLastThreads = {}
         dataLastProcess = numpy.array([], dtype = typeProc)
         num = len(self.dataProcess) - 1
         if num != -1:
           for thread in self.lastThreads:
            dataLastThread = numpy.array([], dtype = typeThread) 
            number = len(self.dataThreads[thread]) - 1
            dataLastThread = numpy.append(dataLastThread, self.dataThreads[thread][number])
            dataLastThreads[thread] = dataLastThread
           dataLastProcess = numpy.append(dataLastProcess, self.dataProcess[num])
           self.function (dataLastProcess, dataLastThreads, self.callbackProcQuery, self.callbackThreadsQuery, self.callbackPrevProcData, self.callbackPrevThreadsData) 
           if sys.platform == 'linux2':
            self.callbackPrevProcData = dataLastProcess
            self.callbackPrevThreadsData = dataLastThreads
            
       if self.collect == True:
         if len(self.dataProcess) != 0:      
           self.getDataProcess = numpy.append(self.getDataProcess, self.dataProcess[len(self.dataProcess) - 1])
           threads = self.lastThreads
           for thread in threads:
             num = len(self.dataThreads[thread]) - 1
             if num != -1:            
               if self.getDataThreads.has_key(thread) == 1:
                 self.getDataThreads[thread] = numpy.append(self.getDataThreads[thread], self.dataThreads[thread][num])
               else:
                 self.getDataThreads[thread] = self.dataThreads[thread][num]
       else:
         self.dataProcess = numpy.array([], dtype = typeProc)
         self.dataThreads = {}

      else:
        self.dataProcess = numpy.array([], dtype = typeProc)
        self.dataThreads = {}
      process = self.process
      self.lock.release()
      time.sleep(self.updateInterval)
      
  def setProcess(self, process):
    """
    setProcess(process)
    Starts or stops collect data
    process: bool
         to start collect data process = True
         to stop collect data process = False
    """
    self.lock.acquire()
    self.process = process  
    self.lock.release()
    
  def setCallback(self, function, procQuery = None, threadsQuery = None):
    """
    setCallback(function, procQuery, threadsQuery)
    Sets callback function
    function: function
         function for callback
    procQuery: list or None
         name list of process counters
    threadsQuery: list or None
         name list of thread counters
    """
    self.lock.acquire()
    self.callbackProcQuery = procQuery
    self.callbackThreadsQuery = threadsQuery
    self.callbackPrevProcData = None
    self.callbackPrevThreadsData = None
    self.function = function    
    self.lock.release()
    
  def clearCallback(self):
    """
    clearCallback()
    Clears callback
    """
    self.lock.acquire()
    self.function = None
    self.callbackPrevProcData = None
    self.callbackPrevThreadsData = None
    self.lock.release()
  

  def startCollectData(self):
    """
    startCollectData()
    Starts collect data for getData function
    """
    self.lock.acquire()
    self.collect = True
    self.getdataPrevProcData = None
    self.getdataPrevThreadsData = None
    self.lock.release()
  
  def stopCollectData(self):
    """
    stopCollectData()
    Stops collect data for getData function
    """
    self.lock.acquire()
    self.collect = False 
    self.lock.release()
  
  
  def getData(self, procQuery = None, threadsQuery = None):
    """
    data = getData(procQuery, threadsQuery)
    Returns collected data
    procQuery: list or None
         name list of process counters
    threadsQuery: list or None
         name list of thread counters
    data: tuple 
         (dataProcess, dataThreads) 
         dataProcess: numpy array
              process data array
         dataThreads: dictionary
              threads data dictionary consists of thread data numpy arrays
    """
    
    self.lock.acquire()
    dataProcess, dataThreads = parseQuery (self.getDataProcess, self.getDataThreads, procQuery, threadsQuery, self.getdataPrevProcData, self.getdataPrevThreadsData)
    
    if sys.platform == 'linux2':
      self.getdataPrevProcData = numpy.array([], dtype = typeProc)
      prevThread = numpy.array([], dtype = typeThread)
      self.getdataPrevThreadsData = {}
      if len(self.getDataProcess)!=0:
        self.getdataPrevProcData = numpy.append(self.getdataPrevProcData,self.getDataProcess[-1] )
        for thread in dataThreads.keys():
          prevThread = numpy.append(prevThread, self.getDataThreads[thread][-1])
          self.getdataPrevThreadsData [thread] = prevThread 
    self.getDataProcess = numpy.array([], dtype = typeProc)
    self.getDataThreads = {}
    self.dataProcess = numpy.array([], dtype = typeProc)
    self.dataThreads = {}
    self.lock.release() 
    return dataProcess, dataThreads
  