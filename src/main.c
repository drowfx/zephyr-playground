#include "sopc_builtintypes.h"
#include "libs2opc_common_config.h"
#include "sopc_common_build_info.h"
#include "sopc_enums.h"
#include "sopc_mem_alloc.h"
#include "sopc_pub_scheduler.h"
#include "sopc_pub_source_variable.h"
#include "sopc_pubsub_conf.h"
#include "sopc_types.h"

#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char logCategory[10];
static void log_UserCallback(const char* timestampUtc,
                             const char* category,
                             const SOPC_Log_Level level,
                             const char* const line)
{
    if (line != NULL && (category == NULL || logCategory[0] == 0 || strcmp(logCategory, category) == 0))
    {
        printf("%s\n", line);
    }
}

static const SOPC_Build_Info build_info = {};
SOPC_Build_Info SOPC_ClientServer_GetBuildInfo()
{
    return build_info;
}

SOPC_Build_Info SOPC_Common_GetBuildInfo()
{
    return build_info;
}


static SOPC_DataValue* GetPubSourceVariable(const OpcUa_ReadValueId* nodesToRead, const int32_t nbValues)
{
    SOPC_DataValue *ret = SOPC_Calloc(nbValues, sizeof(SOPC_DataValue));
    if (ret == NULL) return NULL;

    for (int i = 0;i < nbValues;++i) {
        SOPC_DataValue_Initialize(ret+i);
        ret[i].Value.BuiltInTypeId = SOPC_UInt32_Id;
        ret[i].Value.Value.Uint32 = 0x5555aaaa;
    }
    
    return ret;
}

static const char* SOPC_Platform_Get_Default_Net_Itf(void)
{
    struct net_if* iface = net_if_get_default();
    if (NULL != iface && NULL != iface->if_dev && NULL != iface->if_dev->dev && NULL != iface->if_dev->dev->name)
    {
        return iface->if_dev->dev->name;
    }
    return "";
}

static void SOPC_PubSubConfig_SetPubVariableAt(SOPC_PublishedDataSet* dataset,
                                               uint16_t index,
                                               const char* strNodeId,
                                               SOPC_BuiltinId builtinType)
{
    SOPC_FieldMetaData* fieldmetadata = SOPC_PublishedDataSet_Get_FieldMetaData_At(dataset, index);
    SOPC_PubSub_ArrayDimension arrayDimension = {.valueRank = -1, .arrayDimensions = NULL};
    SOPC_FieldMetaData_ArrayDimension_Move(fieldmetadata, &arrayDimension);
    SOPC_FieldMetaData_Set_BuiltinType(fieldmetadata, builtinType);
    SOPC_PublishedVariable* publishedVar = SOPC_FieldMetaData_Get_PublishedVariable(fieldmetadata);
    assert(NULL != publishedVar);
    SOPC_NodeId* nodeId = SOPC_NodeId_FromCString(strNodeId, (int32_t) strlen(strNodeId));
    assert(NULL != nodeId);
    SOPC_PublishedVariable_Set_NodeId(publishedVar, nodeId);
    SOPC_PublishedVariable_Set_AttributeId(publishedVar,
                                           13); // Value => AttributeId=13
}

static bool setup_pub(SOPC_PubSubConfiguration **pubSubConfig, SOPC_PubSourceVariableConfig **pubSourceConfig)
{
    SOPC_Log_Configuration logConfig;
    logConfig.logSystem = SOPC_LOG_SYSTEM_USER;
    logConfig.logLevel = SOPC_LOG_LEVEL_WARNING;
    logConfig.logSysConfig.userSystemLogConfig.doLog = &log_UserCallback;
    SOPC_ReturnStatus status = SOPC_CommonHelper_Initialize(&logConfig);
    assert(SOPC_STATUS_OK == status && "SOPC_CommonHelper_Initialize failed");

    *pubSourceConfig = SOPC_PubSourceVariableConfig_Create(&GetPubSourceVariable);
    assert(pubSourceConfig != NULL);

    *pubSubConfig = SOPC_PubSubConfiguration_Create();
    assert(pubSubConfig != NULL);

    if (!SOPC_PubSubConfiguration_Allocate_PubConnection_Array(*pubSubConfig, 1)) {
        printf("failed to allocate PubConnection Array\n");
        goto fail;
    }

    SOPC_PubSubConnection *conn = SOPC_PubSubConfiguration_Get_PubConnection_At(*pubSubConfig, 0);
    SOPC_PubSubConnection_Set_PublisherId_UInteger(conn, 42);

    const char *itf_name = SOPC_Platform_Get_Default_Net_Itf();
    printf("default net itf: %s\n", itf_name);
    if (!SOPC_PubSubConnection_Set_InterfaceName(conn, itf_name)) {
        printf("failed to set interface name\n");
        goto fail;
    }

    if (!SOPC_PubSubConnection_Set_Address(conn, "opc.eth://53-66-77-88-99-AA" /* "opc.udp://192.168.10.1:4840" */)) {
        printf("failed to set PubSubConnection address\n");
        goto fail;
    }

    if (!SOPC_PubSubConnection_Allocate_WriterGroup_Array(conn, 1)) {
        printf("failed to allocate WriterGroup array\n");
        goto fail;
    }

    if (!SOPC_PubSubConfiguration_Allocate_PublishedDataSet_Array(*pubSubConfig, 1)) {
        printf("failed to allocate PublishedDataSet array\n");
        goto fail;
    }
        
    SOPC_WriterGroup *group = SOPC_PubSubConnection_Get_WriterGroup_At(conn, 0);
    SOPC_WriterGroup_Set_Id(group, 23);
    SOPC_WriterGroup_Set_Version(group, 0);
    SOPC_WriterGroup_Set_PublishingInterval(group, 500);
    SOPC_WriterGroup_Set_SecurityMode(group, SOPC_SecurityMode_None);
    if (!SOPC_WriterGroup_Allocate_DataSetWriter_Array(group, 1)) {
        printf("failed to allocate DataSetWriter\n");
        goto fail;
    }
    SOPC_DataSetWriter *writer = SOPC_WriterGroup_Get_DataSetWriter_At(group, 0);
    SOPC_DataSetWriter_Set_Id(writer, 23);

    SOPC_PublishedDataSet *dataset = SOPC_PubSubConfiguration_Get_PublishedDataSet_At(*pubSubConfig, 0);
    if (!SOPC_PublishedDataSet_Init(dataset, SOPC_PublishedDataItemsDataType, 1)) {
        printf("failed to init PublishedDataSet\n");
        goto fail;
    }
    SOPC_DataSetWriter_Set_DataSet(writer, dataset);

    SOPC_PubSubConfig_SetPubVariableAt(dataset, 0, "ns=1;s=PubUInt32", SOPC_UInt32_Id);

    return true;

 fail:
    SOPC_PubSubConfiguration_Delete(*pubSubConfig);
    SOPC_PubSourceVariableConfig_Delete(*pubSourceConfig);

    return false;
}

static void free_pub(SOPC_PubSubConfiguration *pubSubConfig, SOPC_PubSourceVariableConfig *pubSourceConfig)
{
    SOPC_PubSubConfiguration_Delete(pubSubConfig);
    SOPC_PubSourceVariableConfig_Delete(pubSourceConfig);
}


int main(void)
{
    SOPC_PubSubConfiguration *pubSubConfig;
    SOPC_PubSourceVariableConfig *pubSourceConfig;

    if (setup_pub(&pubSubConfig, &pubSourceConfig)) {
        if (!SOPC_PubScheduler_Start(pubSubConfig, pubSourceConfig, 18)) {
            free_pub(pubSubConfig, pubSourceConfig);
            printf("failed to start Publisher\n");
        }
    } else {
        printf("failed to setup Publisher\n");
    }
    
    while(true)  {
        sleep(1);
    }

    return 1;
}
