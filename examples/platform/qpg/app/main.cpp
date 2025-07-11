/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/** @file "main.cpp"
 *
 * Main application.
 */

/*****************************************************************************
 *                    Includes Definitions
 *****************************************************************************/

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#if defined(GP_APP_DIVERSITY_POWERCYCLECOUNTING)
#include "powercycle_counting.h"
#endif
#if defined(GP_APP_DIVERSITY_CLEARBOX_TESTING_HOOK_APPLICATION_INIT)
#include "clearbox_testing_hooks.h"
#endif

#if defined(QPG6200)
#include "gpNvm.h"
#endif // QPG6200

// Qorvo CHIP library
#include "qvCHIP.h"

// CHIP includes
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPPlatformMemory.h>
#include <lib/support/logging/CHIPLogging.h>
#include <platform/CHIPDeviceLayer.h>

#include <app/clusters/network-commissioning/network-commissioning.h>
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>

#if defined(PW_RPC_ENABLED) && PW_RPC_ENABLED
#include "Rpc.h"
#endif // PW_RPC_ENABLED

#if defined(ENABLE_CHIP_SHELL) && ENABLE_CHIP_SHELL
#include "shell_common/shell.h"
#endif // ENABLE_CHIP_SHELL
/* access CHIP_CRYPTO_PSA */
#include "crypto/CryptoBuildConfig.h"
#if CHIP_CRYPTO_PSA
#include "psa/crypto.h"
#include "psa/crypto_se.h"
#endif

// Application level logic
#include "AppConfig.h"
#if BASE_APP_BUILD
#include "BaseAppTask.h"
#else
#include "AppTask.h"
#endif
#include "ota.h"

using namespace ::chip;
using namespace ::chip::Inet;
using namespace ::chip::DeviceLayer;
using namespace ::chip::DeviceLayer::Internal;

app::Clusters::NetworkCommissioning::InstanceAndDriver<NetworkCommissioning::GenericThreadDriver>
    sThreadNetworkDriver(chip::kRootEndpointId);

namespace {
constexpr uint32_t kInitOTARequestorDelaySec = 3;
constexpr int extDiscTimeoutSecs             = 20;
} // namespace

/*****************************************************************************
 *                    Macro Definitions
 *****************************************************************************/

/*****************************************************************************
 *                    External Function Definitions
 *****************************************************************************/

/*****************************************************************************
 *                    Application Function Definitions
 *****************************************************************************/

CHIP_ERROR CHIP_Init(void);

void Application_Init(void)
{
    CHIP_ERROR error;

#if defined(GP_APP_DIVERSITY_POWERCYCLECOUNTING)
    gpAppFramework_Reset_Init();
#endif

#if CHIP_CRYPTO_PSA
    psa_status_t psa_error = psa_crypto_init();
    if (psa_error)
    {
        ChipLogError(NotSpecified, "psa_crypto_init failed");
        return;
    }
#endif

    /* Initialize CHIP stack */
    error = CHIP_Init();
    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "CHIP_Init failed");
        return;
    }

    /* Launch application task */
    ChipLogProgress(NotSpecified, "============================");
    ChipLogProgress(NotSpecified, "Qorvo " APP_NAME " Launching");
    ChipLogProgress(NotSpecified, "============================");

#if BASE_APP_BUILD
    BaseAppTask & appTask = BaseAppTask::GetAppTask();
#else
    AppTask & appTask = AppTask::GetAppTask();
#endif
    error = appTask.Init();
    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "GetAppTask().Init() failed");
        return;
    }

    error = appTask.StartAppTask();
    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "GetAppTask().StartAppTask() failed");
        return;
    }

#if defined(GP_APP_DIVERSITY_CLEARBOX_TESTING_HOOK_APPLICATION_INIT)
    /* After init calls so errors there cause the hook not to be run */
    GP_CLEARBOX_TESTING_APPLICATION_INIT_HOOK;
#endif
}

void ChipEventHandler(const ChipDeviceEvent * aEvent, intptr_t /* arg */)
{
    switch (aEvent->Type)
    {
    case DeviceEventType::kDnssdInitialized:
#if CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR
        InitializeOTARequestor();
#endif
        break;
    default:
        break;
    }
}

CHIP_ERROR CHIP_Init(void)
{
    CHIP_ERROR ret = CHIP_NO_ERROR;

#if defined(PW_RPC_ENABLED) && PW_RPC_ENABLED
    ret = (CHIP_ERROR) chip::rpc::Init();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "rpc::Init() failed");
        goto exit;
    }
#endif

    ret = chip::Platform::MemoryInit();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Platform::MemoryInit() failed");
        goto exit;
    }

#if defined(ENABLE_CHIP_SHELL) && ENABLE_CHIP_SHELL
    ret = (CHIP_ERROR) ShellTask::Start();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "ShellTask::Start() failed");
        goto exit;
    }
#endif // ENABLE_CHIP_SHELL

    ret = PlatformMgr().InitChipStack();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "PlatformMgr().InitChipStack() failed");
        goto exit;
    }

#if CHIP_ENABLE_OPENTHREAD
    ChipLogProgress(NotSpecified, "Initializing OpenThread stack");

    ret = ThreadStackMgr().InitThreadStack();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "ThreadStackMgr().InitThreadStack() failed");
        goto exit;
    }

#if defined(CHIP_DEVICE_CONFIG_ENABLE_SSED) && CHIP_DEVICE_CONFIG_ENABLE_SSED
    ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice);
    qvCHIP_EnableSleep(true);
#elif CHIP_DEVICE_CONFIG_ENABLE_SED
    ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_SleepyEndDevice);
    qvCHIP_EnableSleep(true);
#elif CHIP_DEVICE_CONFIG_THREAD_FTD
    ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_Router);
    qvCHIP_DisableSleepAlwaysForDevice();
#else
    ret = ConnectivityMgr().SetThreadDeviceType(ConnectivityManager::kThreadDeviceType_MinimalEndDevice);
    qvCHIP_DisableSleepAlwaysForDevice();
#endif
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "ConnectivityMgr().SetThreadDeviceType() failed");
        goto exit;
    }

    sThreadNetworkDriver.Init();

    ChipLogProgress(NotSpecified, "Starting OpenThread task");
    ret = ThreadStackMgrImpl().StartThreadTask();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "ThreadStackMgr().StartThreadTask() failed");
        goto exit;
    }
#endif // CHIP_ENABLE_OPENTHREAD

    ChipLogProgress(NotSpecified, "Starting Platform Manager Event Loop");
    PlatformMgr().AddEventHandler(ChipEventHandler, 0);
    ret = PlatformMgr().StartEventLoopTask();
    if (ret != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "PlatformMgr().StartEventLoopTask() failed");
        goto exit;
    }

exit:
    return ret;
}

/*****************************************************************************
 * --- Main
 *****************************************************************************/
int main(void)
{
    int result;

    /* Initialize Qorvo stack */
    result = qvCHIP_init(Application_Init);
    if (result < 0)
    {
        return 0;
    }

    /* Start FreeRTOS */
    vTaskStartScheduler();
    return 0;
}
