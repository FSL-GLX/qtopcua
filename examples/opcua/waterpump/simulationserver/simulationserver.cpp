 /****************************************************************************
**
** Copyright (C) 2018 basysKom GmbH, opensource@basyskom.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtOpcUa module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "simulationserver.h"
#include <qopen62541utils.h>
#include <qopen62541valueconverter.h>
#include <QtOpcUa/qopcuatype.h>

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QUuid>

#include <QtCore/QFile>
#include <QtCore/QDir>

#include <cstring>

#if defined UA_ENABLE_ENCRYPTION
static const size_t usernamePasswordsSize = 2;
static UA_UsernamePasswordLogin usernamePasswords[2] = {
    {UA_STRING_STATIC("user1"), UA_STRING_STATIC("password")},
    {UA_STRING_STATIC("user2"), UA_STRING_STATIC("password1")}};
#endif

const UA_UInt16 portNumber = 43344;


QT_BEGIN_NAMESPACE

// Node ID conversion is included from the open62541 plugin but warnings from there should be logged
// using qt.opcua.testserver instead of qt.opcua.plugins.open62541 for usage in the test server
Q_LOGGING_CATEGORY(QT_OPCUA_PLUGINS_OPEN62541, "qt.opcua.demoserver")

//QVariant UAVariantToQVariant(UA_Variant uaVar)
//{
//	QVariant var;
//	if (UA_Variant_isScalar(&uaVar))
//	{
//		switch (uaVar.type->typeIndex){
//		case UA_TYPES_BOOLEAN:
//			var = QVariant(*(UA_Boolean*)uaVar.data);
//			break;
//		case UA_TYPES_SBYTE:
//			var = QVariant(*(UA_SByte*)uaVar.data);
//			break;
//		case UA_TYPES_BYTE:
//			var = QVariant(*(UA_Byte*)uaVar.data);
//			break;
//		case UA_TYPES_INT16:
//			var = QVariant(*(UA_Int16*)uaVar.data);
//			break;
//		case UA_TYPES_UINT16:
//			var = QVariant(*(UA_UInt16*)uaVar.data);
//			break;
//		case UA_TYPES_INT32:
//			var = QVariant(*(UA_Int32*)uaVar.data);
//			break;
//		case UA_TYPES_UINT32:
//			var = QVariant(*(UA_UInt32*)uaVar.data);
//			break;
//		case UA_TYPES_FLOAT:
//			var = QVariant(*(UA_Float*)uaVar.data);
//			break;
//		case UA_TYPES_DOUBLE:
//			var = QVariant(*(UA_Double*)uaVar.data);
//			break;
//		case UA_TYPES_STRING:
//			UA_String ua_string = *(UA_String*)uaVar.data;
//			QString string = QString::fromLatin1((char *)ua_string.data, ua_string.length);
//			var = QVariant(string);
//			break;
//		}
//	}
//	return var;
//}



DemoServer::DemoServer(QObject *parent)
    : QObject(parent)
    , m_state(DemoServer::MachineState::Idle)
    , m_percentFilledTank1(100)
    , m_percentFilledTank2(0)
{
    m_timer.setInterval(0);
    m_timer.setSingleShot(false);
    m_machineTimer.setInterval(200);
    connect(&m_timer, &QTimer::timeout, this, &DemoServer::processServerEvents);
}

DemoServer::~DemoServer()
{
    shutdown();
    UA_Server_delete(m_server);
    UA_NodeId_deleteMembers(&m_percentFilledTank1Node);
    UA_NodeId_deleteMembers(&m_percentFilledTank2Node);
    UA_NodeId_deleteMembers(&m_tank2TargetPercentNode);
    UA_NodeId_deleteMembers(&m_tank2ValveStateNode);
    UA_NodeId_deleteMembers(&m_machineStateNode);
}

bool DemoServer::init()
{
    m_server = UA_Server_new();

    if (!m_server)
        return false;

	m_config = UA_Server_getConfig(m_server);
	//return createInsecureServerConfig(m_config);
	return createSecureServerConfig(m_config);
	//UA_StatusCode result = UA_ServerConfig_setMinimal(UA_Server_getConfig(m_server), 43344, nullptr);

//    if (result != UA_STATUSCODE_GOOD)
//        return false;

//    return true;
}


bool DemoServer::createInsecureServerConfig(UA_ServerConfig *config)
{
	UA_StatusCode result = UA_ServerConfig_setMinimal(config, 43344, nullptr);

	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to create server config without encryption";
		return false;
	}

	// This is needed for COIN because the hostname returned by gethostname() is not resolvable.
	config->customHostname = UA_String_fromChars("localhost");

	return true;
}

#if defined UA_ENABLE_ENCRYPTION
static UA_ByteString loadFile(const QString &filePath) {
	UA_ByteString fileContents = UA_STRING_NULL;
	fileContents.length = 0;

	QFile file(filePath);
	if (!file.open(QFile::ReadOnly))
		return fileContents;

	fileContents.length = file.size();
	fileContents.data = (UA_Byte *)UA_malloc(fileContents.length * sizeof(UA_Byte));
	if (!fileContents.data)
		return fileContents;

	if (file.read(reinterpret_cast<char*>(fileContents.data), fileContents.length) != static_cast<qint64>(fileContents.length)) {
		UA_ByteString_deleteMembers(&fileContents);
		fileContents.length = 0;
		return fileContents;
	}
	return fileContents;
}

bool DemoServer::createSecureServerConfig(UA_ServerConfig *config)
{
	const QString certificateFilePath = QLatin1String(":/pki/own/certs/open62541-testserver.der");
	const QString privateKeyFilePath = QLatin1String(":/pki/own/private/open62541-testserver.der");

	UA_ByteString certificate = loadFile(certificateFilePath);
	UaDeleter<UA_ByteString> certificateDeleter(&certificate, UA_ByteString_deleteMembers);
	UA_ByteString privateKey = loadFile(privateKeyFilePath);
	UaDeleter<UA_ByteString> privateKeyDeleter(&privateKey, UA_ByteString_deleteMembers);

	if (certificate.length == 0) {
		UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
		   "Failed to load certificate %s", certificateFilePath.toLocal8Bit().constData());
		return false;
	}
	if (privateKey.length == 0) {
		UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
		   "Failed to load private key %s", privateKeyFilePath.toLocal8Bit().constData());
		return false;
	}

	// Load the trustlist
	QDir trustDir(":/pki/trusted/certs");
	if (!trustDir.exists()) {
		UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Trust directory does not exist");
		return false;
	}

	const auto trustedCerts = trustDir.entryList(QDir::Files);
	const size_t trustListSize = trustedCerts.size();
	int i = 0;

	UA_STACKARRAY(UA_ByteString, trustList, trustListSize);
	UaArrayDeleter<UA_TYPES_BYTESTRING> trustListDeleter(&trustList, trustListSize);

	for (const auto &entry : trustedCerts) {
		trustList[i] = loadFile(trustDir.filePath(entry));
		if (trustList[i].length == 0) {
			UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Failed to load trusted certificate");
			return false;
		} else {
			UA_LOG_DEBUG(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "Trusted certificate %s loaded", entry.toLocal8Bit().constData());
		}
		++i;
	}

	// Loading of a revocation list currently unsupported
	UA_ByteString *revocationList = nullptr;
	size_t revocationListSize = 0;

	if (trustListSize == 0)
		UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
		  "No CA trust-list provided. Any remote certificate will be accepted.");

	UA_ServerConfig_setBasics(config);

	// This is needed for COIN because the hostname returned by gethostname() is not resolvable.
	config->customHostname = UA_String_fromChars("localhost");

	UA_StatusCode result = UA_CertificateVerification_Trustlist(&config->certificateVerification,
	                                              trustList, trustListSize,
	                                              nullptr, 0,
	                                              revocationList, revocationListSize);
	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to initialize certificate verification";
		return false;
	}

	// Do not delete items on success.
	// They will be used by the server.
	trustListDeleter.release();

	result = UA_ServerConfig_addNetworkLayerTCP(config, portNumber, 0, 0);

	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to add network layer";
		return false;
	}

	result = UA_ServerConfig_addAllSecurityPolicies(config, &certificate, &privateKey);

	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to add security policies";
		return false;
	}

	// Do not delete items on success.
	// They will be used by the server.
	certificateDeleter.release();
	privateKeyDeleter.release();

	result = UA_AccessControl_default(config, true,
	            &config->securityPolicies[0].policyUri,
	            usernamePasswordsSize, usernamePasswords);

	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to create access control";
		return false;
	}

	result = UA_ServerConfig_addAllEndpoints(config);

	if (result != UA_STATUSCODE_GOOD) {
		qWarning() << "Failed to add endpoints";
		return false;
	}

	return true;
}
#endif


void DemoServer::processServerEvents()
{
    if (m_running)
        UA_Server_run_iterate(m_server, true);
}

void DemoServer::shutdown()
{
    if (m_running) {
        UA_Server_run_shutdown(m_server);
        m_running = false;
    }
}

UA_NodeId DemoServer::addObject(const QString &parent, const QString &nodeString, const QString &browseName,
                                const QString &displayName, const QString &description, quint32 referenceType)
{
    UA_NodeId resultNode;
    UA_ObjectAttributes oAttr = UA_ObjectAttributes_default;

    oAttr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
    if (description.size())
        oAttr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", description.toUtf8().constData());

    UA_StatusCode result;
    UA_NodeId requestedNodeId = Open62541Utils::nodeIdFromQString(nodeString);
    UA_NodeId parentNodeId = Open62541Utils::nodeIdFromQString(parent);

    UA_QualifiedName nodeBrowseName = UA_QUALIFIEDNAME_ALLOC(requestedNodeId.namespaceIndex, browseName.toUtf8().constData());

    result = UA_Server_addObjectNode(m_server,
                                     requestedNodeId,
                                     parentNodeId,
                                     UA_NODEID_NUMERIC(0, referenceType),
                                     nodeBrowseName,
                                     UA_NODEID_NULL,
                                     oAttr,
                                     nullptr,
                                     &resultNode);

    UA_QualifiedName_deleteMembers(&nodeBrowseName);
    UA_NodeId_deleteMembers(&requestedNodeId);
    UA_NodeId_deleteMembers(&parentNodeId);
    UA_ObjectAttributes_deleteMembers(&oAttr);

    if (result != UA_STATUSCODE_GOOD) {
        qWarning() << "Could not add folder:" << nodeString << " :" << result;
        return UA_NODEID_NULL;
    }
    return resultNode;
}

UA_NodeId DemoServer::addVariable(const UA_NodeId &folder, const QString &variableNode, const QString &browseName,
                                  const QString &displayName, const QVariant &value, QOpcUa::Types type,
                                  quint32 referenceType)
{
    UA_NodeId variableNodeId = Open62541Utils::nodeIdFromQString(variableNode);

    UA_VariableAttributes attr = UA_VariableAttributes_default;
    attr.value = QOpen62541ValueConverter::toOpen62541Variant(value, type);
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
    attr.dataType = attr.value.type ? attr.value.type->typeId : UA_TYPES[UA_TYPES_BOOLEAN].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_QualifiedName variableName = UA_QUALIFIEDNAME_ALLOC(variableNodeId.namespaceIndex, browseName.toUtf8().constData());

    UA_NodeId resultId;
    UA_StatusCode result = UA_Server_addVariableNode(m_server,
                                                     variableNodeId,
                                                     folder,
                                                     UA_NODEID_NUMERIC(0, referenceType),
                                                     variableName,
                                                     UA_NODEID_NULL,
                                                     attr,
                                                     nullptr,
                                                     &resultId);

    UA_NodeId_deleteMembers(&variableNodeId);
    UA_VariableAttributes_deleteMembers(&attr);
    UA_QualifiedName_deleteMembers(&variableName);

    if (result != UA_STATUSCODE_GOOD) {
        qWarning() << "Could not add variable:" << result;
        return UA_NODEID_NULL;
    }

    return resultId;
}

UA_StatusCode DemoServer::startPumpMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    DemoServer *data = static_cast<DemoServer *>(methodContext);

    double targetValue = data->readTank2TargetValue();

    if (data->m_state == MachineState::Idle && data->m_percentFilledTank1 > 0 && data->m_percentFilledTank2 < targetValue) {
        qDebug() << "Start pumping";
        data->setState(MachineState::Pumping);
        data->m_machineTimer.start();
        return UA_STATUSCODE_GOOD;
    }
    else {
        qDebug() << "Machine already running";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode DemoServer::stopPumpMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    DemoServer *data = static_cast<DemoServer *>(methodContext);

    if (data->m_state == MachineState::Pumping) {
        qDebug() << "Stopping";
        data->m_machineTimer.stop();
        data->setState(MachineState::Idle);
        return UA_STATUSCODE_GOOD;
    } else {
        qDebug() << "Nothing to stop";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode DemoServer::flushTank2Method(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    DemoServer *data = static_cast<DemoServer *>(methodContext);

    double targetValue = data->readTank2TargetValue();

    if (data->m_state == MachineState::Idle && data->m_percentFilledTank2 > targetValue) {
        data->setState(MachineState::Flushing);
        qDebug() << "Flushing tank 2";
        data->setTank2ValveState(true);
        data->m_machineTimer.start();
        return UA_STATUSCODE_GOOD;
    }
    else {
        qDebug() << "Unable to comply";
        return UA_STATUSCODE_BADUSERACCESSDENIED;
    }
}

UA_StatusCode DemoServer::resetMethod(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
    Q_UNUSED(server);
    Q_UNUSED(sessionId);
    Q_UNUSED(sessionHandle);
    Q_UNUSED(methodId);
    Q_UNUSED(objectId);
    Q_UNUSED(objectContext);
    Q_UNUSED(inputSize);
    Q_UNUSED(input);
    Q_UNUSED(outputSize);
    Q_UNUSED(output);

    DemoServer *data = static_cast<DemoServer *>(methodContext);

        qDebug() << "Reset simulation";
        data->setState(MachineState::Idle);
        data->m_machineTimer.stop();
        data->setTank2ValveState(false);
        data->setPercentFillTank1(100);
        data->setPercentFillTank2(0);
		return UA_STATUSCODE_GOOD;
}

UA_StatusCode DemoServer::simulateCommand(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
	Q_UNUSED(server);
	Q_UNUSED(sessionId);
	Q_UNUSED(sessionHandle);
	Q_UNUSED(methodId);
	Q_UNUSED(objectId);
	Q_UNUSED(objectContext);
	Q_UNUSED(inputSize);
	Q_UNUSED(input);
	Q_UNUSED(outputSize);
	Q_UNUSED(output);

	DemoServer *data = static_cast<DemoServer *>(methodContext);

	    auto cmdACK = data->readCmdACK();
		if (cmdACK != 0)
		{
			qDebug() << "Simulate Command - Wrong Cmd_ACK";
			return UA_STATUSCODE_BADUSERACCESSDENIED;
		}
		data->setCmdProgramId("ProgramId1");
		data->setCmdPartId("PartId1");
		data->setCmdWorkOrder("WorkOrder1");
		data->setCmdACK(1);
		qWarning() << "Simulate Command";

		return UA_STATUSCODE_GOOD;
}

UA_StatusCode DemoServer::simulateReceiveResult(UA_Server *server, const UA_NodeId *sessionId, void *sessionHandle, const UA_NodeId *methodId, void *methodContext, const UA_NodeId *objectId, void *objectContext, size_t inputSize, const UA_Variant *input, size_t outputSize, UA_Variant *output)
{
	Q_UNUSED(server);
	Q_UNUSED(sessionId);
	Q_UNUSED(sessionHandle);
	Q_UNUSED(methodId);
	Q_UNUSED(objectId);
	Q_UNUSED(objectContext);
	Q_UNUSED(inputSize);
	Q_UNUSED(input);
	Q_UNUSED(outputSize);
	Q_UNUSED(output);

	DemoServer *data = static_cast<DemoServer *>(methodContext);

	    auto resACK = data->readResACK();
		if (resACK != 1)
		{
			qDebug() << "Simulate Command - Wrong Res_ACK";
			return UA_STATUSCODE_BADUSERACCESSDENIED;
		}
		auto programId = data->readResProgramId();
		auto partId = data->readResPartId();
		auto workOrder = data->readResWorkOrder();
		auto run = data->readResRun();
		auto control = data->readResControl();
		data->setResACK(0);
		qWarning() << "Received Result" << programId << partId << workOrder	<< run << control;

		data->setCmdProgramId("None");
		data->setCmdPartId("None");
		data->setCmdWorkOrder("None");
		return UA_STATUSCODE_GOOD;
}

void DemoServer::setState(DemoServer::MachineState state)
{
    UA_Variant val;
    m_state = state;
    UA_Variant_setScalarCopy(&val, &state, &UA_TYPES[UA_TYPES_UINT32]);
    UA_Server_writeValue(m_server, m_machineStateNode, val);
}

void DemoServer::setPercentFillTank1(double fill)
{
    UA_Variant val;
    m_percentFilledTank1 = fill;
    UA_Variant_setScalarCopy(&val, &fill, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(this->m_server, this->m_percentFilledTank1Node, val);
}

void DemoServer::setPercentFillTank2(double fill)
{
    UA_Variant val;
    m_percentFilledTank2 = fill;
    UA_Variant_setScalarCopy(&val, &fill, &UA_TYPES[UA_TYPES_DOUBLE]);
    UA_Server_writeValue(this->m_server, this->m_percentFilledTank2Node, val);
}

void DemoServer::setTank2ValveState(bool state)
{
    UA_Variant val;
    UA_Variant_setScalarCopy(&val, &state, &UA_TYPES[UA_TYPES_BOOLEAN]);
    UA_Server_writeValue(this->m_server, this->m_tank2ValveStateNode, val);
}

double DemoServer::readTank2TargetValue()
{
    UA_Variant var;
    UA_Server_readValue(m_server, m_tank2TargetPercentNode, &var);
	return static_cast<double *>(var.data)[0];
}

int DemoServer::readCmdACK()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Cmd_ACK, &var);
	return static_cast<int *>(var.data)[0];
}

void DemoServer::setCmdACK(int ack)
{
	UA_Variant val;
	UA_Variant_setScalarCopy(&val, &ack, &UA_TYPES[UA_TYPES_INT32]);
	UA_Server_writeValue(this->m_server, this->m_Cmd_ACK, val);
}

void DemoServer::setCmdProgramId(QString value)
{
	UA_Variant val = QOpen62541ValueConverter::toOpen62541Variant(value, QOpcUa::Types::String);
	UA_Server_writeValue(this->m_server, this->m_Cmd_ProgramId, val);
}

void DemoServer::setCmdPartId(QString value)
{
	UA_Variant val = QOpen62541ValueConverter::toOpen62541Variant(value, QOpcUa::Types::String);
	UA_Server_writeValue(this->m_server, this->m_Cmd_PartId, val);
}

void DemoServer::setCmdWorkOrder(QString value)
{
	UA_Variant val = QOpen62541ValueConverter::toOpen62541Variant(value, QOpcUa::Types::String);
	UA_Server_writeValue(this->m_server, this->m_Cmd_WorkOrder, val);
}

int DemoServer::readResACK()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_ACK, &var);
	return static_cast<int *>(var.data)[0];
}

void DemoServer::setResACK(int ack)
{
	UA_Variant val;
	UA_Variant_setScalarCopy(&val, &ack, &UA_TYPES[UA_TYPES_INT32]);
	UA_Server_writeValue(this->m_server, this->m_Res_ACK, val);
}

QString DemoServer::readResProgramId()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_ProgramId, &var);
	return QOpen62541ValueConverter::toQVariant(var).toString();
}

QString DemoServer::readResPartId()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_PartId, &var);
	return QOpen62541ValueConverter::toQVariant(var).toString();
}

QString DemoServer::readResWorkOrder()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_WorkOrder, &var);
	return QOpen62541ValueConverter::toQVariant(var).toString();
}

int DemoServer::readResRun()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_Run, &var);
	return static_cast<int *>(var.data)[0];
}

int DemoServer::readResControl()
{
	UA_Variant var;
	UA_Server_readValue(m_server, m_Res_Control, &var);
	return static_cast<int *>(var.data)[0];
}

UA_NodeId DemoServer::addMethod(const UA_NodeId &folder, const QString &variableNode, const QString &description,
                                const QString &browseName, const QString &displayName, UA_MethodCallback cb,
                                quint32 referenceType)
{
    UA_NodeId methodNodeId = Open62541Utils::nodeIdFromQString(variableNode);

    UA_MethodAttributes attr = UA_MethodAttributes_default;

    attr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", description.toUtf8().constData());
    attr.displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", displayName.toUtf8().constData());
    attr.executable = true;
    UA_QualifiedName methodBrowseName = UA_QUALIFIEDNAME_ALLOC(methodNodeId.namespaceIndex, browseName.toUtf8().constData());

    UA_NodeId resultId;
    UA_StatusCode result = UA_Server_addMethodNode(m_server, methodNodeId, folder,
                                                     UA_NODEID_NUMERIC(0, referenceType),
                                                     methodBrowseName,
                                                     attr, cb,
                                                     0, nullptr,
                                                     0, nullptr,
                                                     this, &resultId);

    UA_NodeId_deleteMembers(&methodNodeId);
    UA_MethodAttributes_deleteMembers(&attr);
    UA_QualifiedName_deleteMembers(&methodBrowseName);

    if (result != UA_STATUSCODE_GOOD) {
        qWarning() << "Could not add Method:" << result;
        return UA_NODEID_NULL;
    }
    return resultId;
}

void DemoServer::launch()
{
    UA_StatusCode s = UA_Server_run_startup(m_server);
    if (s != UA_STATUSCODE_GOOD)
         qFatal("Could not launch server");
     m_running = true;
     m_timer.start();

     int ns1 = UA_Server_addNamespace(m_server, "Demo Namespace");
     if (ns1 != 2) {
         qFatal("Unexpected namespace index for Demo namespace");
     }

     UA_NodeId machineObject = addObject(QOpcUa::namespace0Id(QOpcUa::NodeIds::Namespace0::ObjectsFolder), "ns=2;s=Machine",
                                         "Machine", "Machine", "The machine simulator", UA_NS0ID_ORGANIZES);
     UA_NodeId tank1Object = addObject("ns=2;s=Machine", "ns=2;s=Machine.Tank1", "Tank1", "Tank 1");
     UA_NodeId tank2Object = addObject("ns=2;s=Machine", "ns=2;s=Machine.Tank2", "Tank2", "Tank 2");

     m_percentFilledTank1Node = addVariable(tank1Object, "ns=2;s=Machine.Tank1.PercentFilled", "PercentFilled", "Tank 1 Fill Level", 100.0, QOpcUa::Types::Double);
     m_percentFilledTank2Node = addVariable(tank2Object, "ns=2;s=Machine.Tank2.PercentFilled", "PercentFilled", "Tank 2 Fill Level", 0.0, QOpcUa::Types::Double);
     m_tank2TargetPercentNode = addVariable(tank2Object, "ns=2;s=Machine.Tank2.TargetPercent", "TargetPercent", "Tank 2 Target Level", 0.0, QOpcUa::Types::Double);
     m_tank2ValveStateNode = addVariable(tank2Object, "ns=2;s=Machine.Tank2.ValveState", "ValveState", "Tank 2 Valve State", false, QOpcUa::Types::Boolean);
     m_machineStateNode = addVariable(machineObject, "ns=2;s=Machine.State", "State", "Machine State", static_cast<quint32>(MachineState::Idle), QOpcUa::Types::UInt32);
     UA_NodeId tempId;
     tempId = addVariable(machineObject, "ns=2;s=Machine.Designation", "Designation", "Machine Designation", "TankExample", QOpcUa::Types::String);
     UA_NodeId_deleteMembers(&tempId);

     tempId = addMethod(machineObject, "ns=2;s=Machine.Start", "Starts the pump", "Start", "Start Pump", &startPumpMethod);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.Stop", "Stops the pump", "Stop", "Stop Pump", &stopPumpMethod);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.FlushTank2", "Flushes tank 2", "FlushTank2", "Flush Tank 2", &flushTank2Method);
     UA_NodeId_deleteMembers(&tempId);
     tempId = addMethod(machineObject, "ns=2;s=Machine.Reset", "Resets the simulation", "Reset", "Reset Simulation", &resetMethod);
     UA_NodeId_deleteMembers(&tempId);

     UA_NodeId_deleteMembers(&machineObject);
     UA_NodeId_deleteMembers(&tank1Object);
     UA_NodeId_deleteMembers(&tank2Object);

     QObject::connect(&m_machineTimer, &QTimer::timeout, [this]() {

         double targetValue = readTank2TargetValue();
         if (m_state == MachineState::Pumping && m_percentFilledTank1 > 0 && m_percentFilledTank2 < targetValue) {
            setPercentFillTank1(m_percentFilledTank1 - 1);
            setPercentFillTank2(m_percentFilledTank2 + 1);
            if (qFuzzyIsNull(m_percentFilledTank1) || m_percentFilledTank2 >= targetValue) {
                setState(MachineState::Idle);
                m_machineTimer.stop();
            }
         } else if (m_state == MachineState::Flushing && m_percentFilledTank2 > targetValue) {
             setPercentFillTank2(m_percentFilledTank2 - 1);
             if (m_percentFilledTank2 <= targetValue) {
                 setTank2ValveState(false);
                 setState(MachineState::Idle);
                 m_machineTimer.stop();
             }
         }
     });


	 initApriso();


}

void DemoServer::initApriso()
{
	 int ns1 = UA_Server_addNamespace(m_server, "Apriso Namespace");
	 if (ns1 != 3) {
		 qFatal("Unexpected namespace index for Demo namespace");
	 }

	 UA_NodeId aprisoObject = addObject(QOpcUa::namespace0Id(QOpcUa::NodeIds::Namespace0::ObjectsFolder), "ns=3;s=Apriso",
	                                     "Apriso", "Apriso", "The Apriso simulator", UA_NS0ID_ORGANIZES);

	 m_Cmd_ACK = addVariable(aprisoObject, "ns=3;s=Apriso.Cmd_ACK", "Cmd_ACK", "Command Ack", 0, QOpcUa::Types::Int32);
	 m_Cmd_ProgramId = addVariable(aprisoObject, "ns=3;s=Apriso.Cmd_ProgramId", "Cmd_ProgramId", "Command ProgramId", "Program", QOpcUa::Types::String);
	 m_Cmd_PartId = addVariable(aprisoObject, "ns=3;s=Apriso.Cmd_PartId", "Cmd_PartId", "Command PartId", "Part", QOpcUa::Types::String);
	 m_Cmd_WorkOrder = addVariable(aprisoObject, "ns=3;s=Apriso.Cmd_WorkOrder", "Cmd_WorkOrder", "Command WorkOrder", "WO", QOpcUa::Types::String);

	 m_Res_ACK = addVariable(aprisoObject, "ns=3;s=Apriso.Res_ACK", "Res_ACK", "Result Ack", 0, QOpcUa::Types::Int32);
	 m_Res_ProgramId = addVariable(aprisoObject, "ns=3;s=Apriso.Res_ProgramId", "Res_ProgramId", "Result ProgramId", "", QOpcUa::Types::String);
	 m_Res_PartId = addVariable(aprisoObject, "ns=3;s=Apriso.Res_PartId", "Res_PartId", "Result PartId", "", QOpcUa::Types::String);
	 m_Res_WorkOrder = addVariable(aprisoObject, "ns=3;s=Apriso.Res_WorkOrder", "Res_WorkOrder", "Result WorkOrder", "", QOpcUa::Types::String);
	 m_Res_Run = addVariable(aprisoObject, "ns=3;s=Apriso.Res_Run", "Res_Run", "Result Run", 0, QOpcUa::Types::Int32);
	 m_Res_Control = addVariable(aprisoObject, "ns=3;s=Apriso.Res_Control", "Res_Control", "Result Control", 0, QOpcUa::Types::Int32);

	 UA_NodeId tempId = addMethod(aprisoObject, "ns=3;s=Apriso.simulateCommand", "Simulate Command", "Command", "Simulate Command", &simulateCommand);
	 UA_NodeId_deleteMembers(&tempId);
	 tempId = addMethod(aprisoObject, "ns=3;s=Apriso.simulateReceiveResult", "Receive Result", "Result", "Receive Result", &simulateReceiveResult);
	 UA_NodeId_deleteMembers(&tempId);


	 UA_NodeId_deleteMembers(&aprisoObject);
}

QT_END_NAMESPACE
