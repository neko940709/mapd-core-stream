/*
 * Copyright 2017 MapD Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CUDAMGR_H
#define CUDAMGR_H

#include <cstdlib>
#include <vector>
#include <thread>
#include <map>
#ifdef HAVE_CUDA
#include <cuda.h>
#else
#include "../Shared/nocuda.h"
#endif  // HAVE_CUDA

namespace CudaMgr_Namespace {

struct DeviceProperties {
  CUdevice device;
  int computeMajor;
  int computeMinor;
  size_t globalMem;
  int constantMem;
  int sharedMemPerBlock;
  int numMPs;
  int warpSize;
  int maxThreadsPerBlock;
  int maxRegistersPerBlock;
  int maxRegistersPerMP;
  int pciBusId;
  int pciDeviceId;
  int memoryClockKhz;
  int memoryBusWidth;  // in bits
  float memoryBandwidthGBs;
  int clockKhz;
};

class StreamInfo{
public:
    CUstream* streams_;
    CUevent* events_;
    std::map<std::thread::id,int> td_to_sm_;

    int nItems_;
    bool flag_{false};

    CUstream* get_stream_from_td(const std::thread::id td_id);
    void freeStreamInfo();
};


class CudaMgr {
 public:
  CudaMgr(const int numGpus, const int startGpu = 0);
  ~CudaMgr();
  void setContext(const int deviceNum) const;
  void printDeviceProperties() const;
  int8_t* allocatePinnedHostMem(const size_t numBytes);
  int8_t* allocateDeviceMem(const size_t numBytes, const int deviceNum);
  void freePinnedHostMem(int8_t* hostPtr);
  void freeDeviceMem(int8_t* devicePtr);
  void copyHostToDevice(int8_t* devicePtr, const int8_t* hostPtr, const size_t numBytes, const int deviceNum);
  void copyDeviceToHost(int8_t* hostPtr, const int8_t* devicePtr, const size_t numBytes, const int deviceNum);
  //SUNNY:STREAM USE
  void copyHostToDeviceAsync(int8_t* devicePtr, const int8_t* hostPtr, const size_t numBytes, const int deviceNum);
  void copyDeviceToHostAsync(int8_t* hostPtr, const int8_t* devicePtr, const size_t numBytes, const int deviceNum);
  void copyDeviceToDevice(int8_t* destPtr,
                          int8_t* srcPtr,
                          const size_t numBytes,
                          const int destDeviceNum,
                          const int srcDeviceNum);
  void zeroDeviceMem(int8_t* devicePtr, const size_t numBytes, const int deviceNum);
  void setDeviceMem(int8_t* devicePtr, const unsigned char uc, const size_t numBytes, const int deviceNum);
  inline int getDeviceCount() const { return deviceCount_; }
  inline bool isArchMaxwell() const { return (getDeviceCount() > 0 && deviceProperties[0].computeMajor == 5); }
  inline bool isArchPascal() const { return (getDeviceCount() > 0 && deviceProperties[0].computeMajor == 6); }
  std::vector<DeviceProperties> deviceProperties;

  const std::vector<CUcontext>& getDeviceContexts() const { return deviceContexts; }

  //SUNNY: Manages the info of streams
  StreamInfo s_info_;

 private:
  void fillDeviceProperties();
  void createDeviceContexts();
  void checkError(CUresult cuResult);

  int deviceCount_;
#ifdef HAVE_CUDA
  int startGpu_;
#endif  // HAVE_CUDA
  std::vector<CUcontext> deviceContexts;

};  // class CudaMgr

}  // Namespace CudaMgr_Namespace

#endif  // CUDAMGR_H
