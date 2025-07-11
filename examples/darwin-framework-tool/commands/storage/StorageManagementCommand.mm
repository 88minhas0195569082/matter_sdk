/*
 *   Copyright (c) 2022 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include "../common/CHIPCommandStorageDelegate.h"
#include "../common/CommissionerInfos.h"
#include "../common/ControllerStorage.h"
#include "../common/PreferencesStorage.h"

#include "StorageManagementCommand.h"

#import <Matter/Matter.h>

namespace {
NSArray<NSString *> * GetDomains()
{
    __auto_type * domains = @[
        kDarwinFrameworkToolCertificatesDomain,
        kDarwinFrameworkToolControllerDomain
    ];

    return domains;
}
}

CHIP_ERROR StorageClearAll::Run()
{
    __auto_type * domains = GetDomains();
    for (NSString * domain in domains) {
        __auto_type * storage = [[PreferencesStorage alloc] initWithDomain:domain];
        VerifyOrReturnError([storage reset], CHIP_ERROR_INTERNAL);
    }

    return CHIP_NO_ERROR;
}

CHIP_ERROR StorageViewAll::Run()
{
    if (!mCommissionerName.HasValue()) {
        __auto_type * domains = GetDomains();
        for (NSString * domain in domains) {
            __auto_type * storage = [[PreferencesStorage alloc] initWithDomain:domain];
            [storage print];
        }

        return CHIP_NO_ERROR;
    }

    const char * commissionerName = mCommissionerName.Value();
    __auto_type * fabricId = GetCommissionerFabricId(commissionerName);
    __auto_type * uuidString = [NSString stringWithFormat:@"%@%@", @(kControllerIdPrefix), fabricId];
    __auto_type * controllerId = [[NSUUID alloc] initWithUUIDString:uuidString];
    __auto_type * storage = [[ControllerStorage alloc] initWithControllerID:controllerId];
    [storage print];

    return CHIP_NO_ERROR;
}
