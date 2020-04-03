/******************************************************************************
 **  Copyright (c) Raoul Hecky. All Rights Reserved.
 **
 **  Moolticute is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Moolticute is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#ifndef MPDEVICE_H
#define MPDEVICE_H

#include <QObject>
#include "Common.h"
#include "MooltipassCmds.h"
#include "QtHelper.h"
#include "AsyncJobs.h"
#include "MPNode.h"
#include "FilesCache.h"
#include "DeviceSettings.h"
#include "MPSettingsMini.h"

using MPCommandCb = std::function<void(bool success, const QByteArray &data, bool &done)>;
using MPDeviceProgressCb = std::function<void(const QVariantMap &data)>;
using NodeList = QList<MPNode *>;
/* Example usage of the above function
 * MPDeviceProgressCb cb;
 * QVariantMap progressData = { {"total", 100},
 *                      {"current", 2},
 *                      {"msg", "message with %1 arguments: %2 %3"},
 *                      {"msg_args", QVariantList({1, "23", "23423"})}};
 * cb(data)
*/
using MessageHandlerCb = std::function<void(bool success, QString errstr)>;
using MessageHandlerCbData = std::function<void(bool success, QString errstr, QByteArray data)>;

class MPDeviceBleImpl;
class IMessageProtocol;

class MPCommand
{
public:
    QVector<QByteArray> data;
    MPCommandCb cb;
    bool running = false;

    QTimer *timerTimeout = nullptr;
    int retry = CMD_MAX_RETRY;
    int retries_done = 0;
    qint64 sent_ts = 0;

    bool checkReturn = true;

    // For BLE
    QByteArray response;
    int responseSize = 0;
};

class MPDevice: public QObject
{
    Q_OBJECT

    QT_WRITABLE_PROPERTY(Common::MPStatus, status, Common::UnknownStatus)
    QT_WRITABLE_PROPERTY(bool, memMgmtMode, false)
    QT_WRITABLE_PROPERTY(QString, hwVersion, QString())
    QT_WRITABLE_PROPERTY(quint32, serialNumber, 0)              // serial number if firmware is above 1.2
    QT_WRITABLE_PROPERTY(int, flashMbSize, 0)

    QT_WRITABLE_PROPERTY(quint32, credentialsDbChangeNumber, 0)  // credentials db change number
    QT_WRITABLE_PROPERTY(quint32, dataDbChangeNumber, 0)         // data db change number
    QT_WRITABLE_PROPERTY(QByteArray, cardCPZ, QByteArray())     // Card CPZ

    QT_WRITABLE_PROPERTY(qint64, uid, -1)

public:
    MPDevice(QObject *parent);
    virtual ~MPDevice();

    friend class MPDeviceBleImpl;
    friend class MPSettingsMini;

    void setupMessageProtocol();
    void sendInitMessages();
    /* Send a command with data to the device */
    void sendData(MPCmd::Command cmd, const QByteArray &data = QByteArray(), quint32 timeout = CMD_DEFAULT_TIMEOUT, MPCommandCb cb = [](bool, const QByteArray &, bool &){}, bool checkReturn = true);
    void sendData(MPCmd::Command cmd, quint32 timeout, MPCommandCb cb);
    void sendData(MPCmd::Command cmd, MPCommandCb cb);
    void sendData(MPCmd::Command cmd, const QByteArray &data, MPCommandCb cb);
    void enqueueAndRunJob(AsyncJobs* jobs);

    void getUID(const QByteArray & key);

    void processStatusChange(const QByteArray &data);

    //mem mgmt mode
    //cbFailure is used to propagate an error to clients when entering mmm
    void startMemMgmtMode(bool wantData,
                          const MPDeviceProgressCb &cbProgress,
                          const std::function<void(bool success, int errCode, QString errMsg)> &cb);
    void exitMemMgmtMode(bool setMMMBool = true);
    void startIntegrityCheck(const std::function<void(bool success, int freeBlocks, int totalBlocks, QString errstr)> &cb,
                             const MPDeviceProgressCb &cbProgress);

    //Send current date to MP
    void setCurrentDate();

    //Get database change numbers
    void getChangeNumbers();

    //Ask a password for specified service/login to MP
    void getCredential(QString service, const QString &login, const QString &fallback_service, const QString &reqid,
                       std::function<void(bool success, QString errstr, const QString &_service, const QString &login, const QString &pass, const QString &desc)> cb);

    //Add or Set service/login/pass/desc in MP
    void setCredential(QString service, const QString &login,
                       const QString &pass, const QString &description, bool setDesc,
                       MessageHandlerCb cb);

    //Delete credential in MMM and leave
    void delCredentialAndLeave(QString service, const QString &login,
                               const MPDeviceProgressCb &cbProgress,
                               MessageHandlerCb cb);

    //get 32 random bytes from device
    void getRandomNumber(std::function<void(bool success, QString errstr, const QByteArray &nums)> cb);

    //Send a cancel request to device
    void cancelUserRequest(const QString &reqid);
    void writeCancelRequest();

    //Request for a raw data node from the device
    void getDataNode(QString service, const QString &fallback_service, const QString &reqid,
                     std::function<void(bool success, QString errstr, QString service, QByteArray rawData)> cb,
                     const MPDeviceProgressCb &cbProgress);

    //Set data to a context on the device
    void setDataNode(QString service, const QByteArray &nodeData,
                     MessageHandlerCb cb,
                     const MPDeviceProgressCb &cbProgress);

    //Delete a data context from the device
    void deleteDataNodesAndLeave(QStringList services,
                                 MessageHandlerCb cb,
                                 const MPDeviceProgressCb &cbProgress);

    //Check is credential/data node exists
    void serviceExists(bool isDatanode, QString service, const QString &reqid,
                       std::function<void(bool success, QString errstr, QString service, bool exists)> cb);

    // Import unencrypted credentials from CSV
    void importFromCSV(const QJsonArray &creds, const MPDeviceProgressCb &cbProgress,
                          MessageHandlerCb cb);

    //Set full list of credentials in MMM
    void setMMCredentials(const QJsonArray &creds, bool noDelete, const MPDeviceProgressCb &cbProgress,
                          MessageHandlerCb cb);

    //Export database
    void exportDatabase(const QString &encryption, std::function<void(bool success, QString errstr, QByteArray fileData)> cb,
                        const MPDeviceProgressCb &cbProgress);
    //Import database
    void importDatabase(const QByteArray &fileData, bool noDelete,
                        MessageHandlerCb cb,
                        const MPDeviceProgressCb &cbProgress);

    // Reset smart card
    void resetSmartCard(MessageHandlerCb cb);

    // Lock the devide.
    void lockDevice(const MessageHandlerCb &cb);

    void getAvailableUsers(const MessageHandlerCb &cb);

    // Returns bleImpl
    MPDeviceBleImpl* ble() const;
    DeviceSettings* settings() const;

    IMessageProtocol* getMesProt() const;

    //After successfull mem mgmt mode, clients can query data
    NodeList &getLoginNodes() { return loginNodes; }
    NodeList &getDataNodes() { return dataNodes; }

    //true if device is a mini
    inline bool isMini() const { return DeviceType::MINI == deviceType; }
    //true if device is a ble
    inline bool isBLE() const { return DeviceType::BLE == deviceType;}
    //true if device fw version is at least 1.2
    inline bool isFw12() const { return isFw12Flag; }
    //true if device is connected with Bluetooth
    inline bool isBT() const { return isBluetooth; }

    QList<QVariantMap> getFilesCache();
    bool hasFilesCache();
    bool isFilesCacheInSync() const;
    void getStoredFiles(std::function<void(bool, QList<QVariantMap>)> cb);
    void updateFilesCache();
    void addFileToCache(QString fileName, int size);
    void updateFileInCache(QString fileName, int size);
    void removeFileFromCache(QString fileName);

    void loadParams() { pSettings->loadParameters(); }

protected:
    enum ExportPayloadData
    {
        EXPORT_CTR_INDEX = 0,
        EXPORT_CPZ_CTR_INDEX = 1,
        EXPORT_STARTING_PARENT_INDEX = 2,
        EXPORT_DATA_STARTING_PARENT_INDEX = 3,
        EXPORT_FAVORITES_INDEX = 4,
        EXPORT_SERVICE_NODES_INDEX = 5,
        EXPORT_SERVICE_CHILD_NODES_INDEX = 6,
        EXPORT_MC_SERVICE_NODES_INDEX = 7,
        EXPORT_MC_SERVICE_CHILD_NODES_INDEX = 8,
        EXPORT_DEVICE_VERSION_INDEX = 9,
        EXPORT_BUNDLE_VERSION_INDEX = 10,
        EXPORT_CRED_CHANGE_NUMBER_INDEX = 11,
        EXPORT_DATA_CHANGE_NUMBER_INDEX = 12,
        EXPORT_DB_MINI_SERIAL_NUM_INDEX = 13,
        EXPORT_IS_BLE_INDEX = 14,
        EXPORT_BLE_USER_CATEGORIES_INDEX = 15,
        EXPORT_WEBAUTHN_NODES_INDEX = 16,
        EXPORT_WEBAUTHN_CHILD_NODES_INDEX = 17,
        EXPORT_SECURITY_SETTINGS_INDEX = 18,
        EXPORT_USER_LANG_INDEX = 19,
        EXPORT_BT_LAYOUT_INDEX = 20,
        EXPORT_USB_LAYOUT_INDEX = 21
    };

signals:
    /* Signal emited by platform code when new data comes from MP */
    /* A signal is used for platform code that uses a dedicated thread */
    void platformDataRead(const QByteArray &data);

    /* the command has failed in platform code */
    void platformFailed();
    void filesCacheChanged();
    void dbChangeNumbersChanged(const int credentialsDbChangeNumber, const int dataDbChangeNumber);

private slots:
    void newDataRead(const QByteArray &data);
    void commandFailed();
    void sendDataDequeue(); //execute commands from the command queue
    void runAndDequeueJobs(); //execute AsyncJobs from the jobs queues
    void resetFlipBit();

    //Get current card CPZ
    void getCurrentCardCPZ();
    void sendSetupDeviceMessages();
    void handleDeviceUnlocked();


private:
    /* Platform function for starting a read, should be implemented in platform class */
    virtual void platformRead() {}

    /* Platform function for writing data, should be implemented in platform class */
    virtual void platformWrite(const QByteArray &data) { Q_UNUSED(data) }

    void addTimerJob(int msec);
    void loadLoginNode(AsyncJobs *jobs, const QByteArray &address,
                       const MPDeviceProgressCb &cbProgress, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    void loadLoginChildNode(AsyncJobs *jobs, MPNode *parent, MPNode *parentClone,
                            const QByteArray &address, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    void loadDataNode(AsyncJobs *jobs, const QByteArray &address, bool load_childs,
                      const MPDeviceProgressCb &cbProgress);
    void loadDataChildNode(AsyncJobs *jobs, MPNode *parent, MPNode *parentClone, const QByteArray &address, const MPDeviceProgressCb &cbProgress, quint32 nbBytesFetched);
    void loadSingleNodeAndScan(AsyncJobs *jobs, const QByteArray &address,
                               const MPDeviceProgressCb &cbProgress);

    void createJobAddContext(const QString &service, AsyncJobs *jobs, bool isDataNode = false);

    bool getDataNodeCb(AsyncJobs *jobs,
                       const MPDeviceProgressCb &cbProgress,
                       const QByteArray &data, bool &done);
    bool setDataNodeCb(AsyncJobs *jobs, int current,
                       const MPDeviceProgressCb &cbProgress,
                       const QByteArray &data, bool &done);

    inline int getParentNodeSize() const { return pMesProt->getParentNodeSize(); }
    inline int getChildNodeSize() const { return pMesProt->getChildNodeSize(); }

    // Functions added by mathieu for MMM
    void memMgmtModeReadFlash(AsyncJobs *jobs, bool fullScan, const MPDeviceProgressCb &cbProgress, bool getCreds, bool getData, bool getDataChilds);
    MPNode *findNodeWithAddressInList(NodeList list, const QByteArray &address, const quint32 virt_addr = 0);
    MPNode* findCredParentNodeGivenChildNodeAddr(const QByteArray &address, const quint32 virt_addr);
    void addWriteNodePacketToJob(AsyncJobs *jobs, const QByteArray &address, const QByteArray &data, std::function<void(void)> writeCallback);
    void startImportFileMerging(const MPDeviceProgressCb &progressCb, MessageHandlerCb cb, bool noDelete);
    bool checkImportedLoginNodes(const MessageHandlerCb &cb, Common::AddressType addrType);
    bool checkImportedDataNodes(const MessageHandlerCb &cb);
    void loadFreeAddresses(AsyncJobs *jobs, const QByteArray &addressFrom, bool discardFirstAddr, const MPDeviceProgressCb &cbProgress);
    void incrementNeededAddresses(MPNode::NodeType type);
    MPNode *findNodeWithAddressWithGivenParentInList(NodeList list,  MPNode *parent, const QByteArray &address, const quint32 virt_addr);
    MPNode *findNodeWithLoginWithGivenParentInList(NodeList list,  MPNode *parent, const QString& name);
    MPNode *findNodeWithNameInList(NodeList list, const QString& name, bool isParent);
    void deletePossibleFavorite(QByteArray parentAddr, QByteArray childAddr);
    bool finishImportFileMerging(QString &stringError, bool noDelete);
    bool finishImportLoginNodes(QString &stringError, Common::AddressType addrType);
    QByteArray getNextNodeAddressInMemory(const QByteArray &address);
    quint16 getFlashPageFromAddress(const QByteArray &address);
    MPNode *findNodeWithServiceInList(const QString &service, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    QByteArray getMemoryFirstNodeAddress(void);
    quint16 getNumberOfPages(void);
    quint16 getNodesPerPage(void);
    void detagPointedNodes(Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool tagFavoriteNodes(void);

    // Functions added by mathieu for MMM : checks & repairs
    bool addOrphanParentToDB(MPNode *parentNodePt, bool isDataParent, bool addPossibleChildren, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool checkLoadedNodes(bool checkCredentials, bool checkData, bool repairAllowed);
    void checkLoadedLoginNodes(quint32 &parentNum, quint32 &childNum, bool repairAllowed, Common::AddressType addrType);
    bool tagPointedNodes(bool tagCredentials, bool tagData, bool repairAllowed, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool addOrphanParentChildsToDB(MPNode *parentNodePt, bool isDataParent, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool removeEmptyParentFromDB(MPNode* parentNodePt, bool isDataParent, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool readExportFile(const QByteArray &fileData, QString &errorString);
    void readExportNodes(QJsonArray &&nodes, ExportPayloadData id);
    bool readExportPayload(QJsonArray dataArray, QString &errorString);
    bool removeChildFromDB(MPNode* parentNodePt, MPNode* childNodePt, bool deleteEmptyParent, bool deleteFromList, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool addChildToDB(MPNode* parentNodePt, MPNode* childNodePt, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool deleteDataParentChilds(MPNode *parentNodePt);
    MPNode* addNewServiceToDB(const QString &service, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    bool addOrphanChildToDB(MPNode* childNodePt, Common::AddressType addrType = Common::CRED_ADDR_IDX);
    QByteArray generateExportFileData(const QString &encryption = "none");
    void cleanImportedVars(void);
    void cleanMMMVars(void);

    // Functions added by mathieu for unit testing
    bool testCodeAgainstCleanDBChanges(AsyncJobs *jobs);

    // Generate save packets
    bool generateSavePackets(AsyncJobs *jobs, bool tackleCreds, bool tackleData, const MPDeviceProgressCb &cbProgress);
    bool checkModifiedSavePacketNodes(AsyncJobs *jobs, std::function<void()> writeCb, Common::AddressType addrType);
    bool checkRemovedSavePacketNodes(AsyncJobs *jobs, std::function<void()> writeCb, Common::AddressType addrType);

    QByteArray getFreeAddress(quint32 virtualAddr);
    // once we fetched free addresses, this function is called
    void changeVirtualAddressesToFreeAddresses();

    void updateChangeNumbers(AsyncJobs *jobs, quint8 flags);

    // Crypto
    quint64 getUInt64EncryptionKey(const QString &encryption);
    quint64 getUInt64EncryptionKey();
    quint64 getUInt64EncryptionKeyOld();
    QString encryptSimpleCrypt(const QByteArray &data, const QString &encryption);
    QByteArray decryptSimpleCrypt(const QString &payload, const QString &encryption);

    // Last page scanned
    quint16 lastFlashPageScanned = 0;

    //timer that asks status
    QTimer *statusTimer = nullptr;

    //local vars for performance diagnostics
    qint64 diagLastSecs;
    quint32 diagNbBytesRec;
    quint32 diagLastNbBytesPSec;
    int diagFreeBlocks = 0;
    int diagTotalBlocks = 0;

    //command queue
    QQueue<MPCommand> commandQueue;

    //passwords we need to change after leaving mmm
    QList<QStringList> mmmPasswordChangeArray;

    // Number of new addresses we need
    quint32 newAddressesNeededCounter = 0;
    quint32 newAddressesReceivedCounter = 0;

    // Buffer containing the free addresses we will need
    QList<QByteArray> freeAddresses;

    // Values loaded when needed (e.g. mem mgmt mode)
    QByteArray ctrValue;
    QVector<QByteArray> startNode = {{MPNode::EmptyAddress},{MPNode::EmptyAddress}};
    QVector<quint32> virtualStartNode = {0, 0};
    QByteArray startDataNode = MPNode::EmptyAddress;
    quint32 virtualDataStartNode = 0;
    QList<QByteArray> cpzCtrValue;
    QList<QByteArray> favoritesAddrs;
    NodeList loginNodes;         //list of all parent nodes for credentials
    NodeList loginChildNodes;    //list of all parent nodes for credentials
    NodeList dataNodes;          //list of all parent nodes for data nodes
    NodeList dataChildNodes;     //list of all parent nodes for data nodes

    // Payload to send when we need to add an unknown card
    QByteArray unknownCardAddPayload;

    // Clones of these values, used when modifying them in MMM
    quint32 credentialsDbChangeNumberClone;
    quint32 dataDbChangeNumberClone;
    QByteArray ctrValueClone;
    QVector<QByteArray> startNodeClone = {{MPNode::EmptyAddress},{MPNode::EmptyAddress}};
    QByteArray startDataNodeClone = MPNode::EmptyAddress;
    QList<QByteArray> cpzCtrValueClone;
    QList<QByteArray> favoritesAddrsClone;
    NodeList loginNodesClone;         //list of all parent nodes for credentials
    NodeList loginChildNodesClone;    //list of all parent nodes for credentials
    NodeList dataNodesClone;          //list of all parent nodes for data nodes
    NodeList dataChildNodesClone;     //list of all parent nodes for data nodes

    // Imported values
    bool isMooltiAppImportFile;
    quint32 moolticuteImportFileVersion;
    quint8 importedCredentialsDbChangeNumber;
    quint8 importedDataDbChangeNumber;
    quint32 importedDbMiniSerialNumber;
    QByteArray importedCtrValue;
    QByteArray importedStartNode;
    QByteArray importedStartDataNode;
    QList<QByteArray> importedCpzCtrValue;
    QList<QByteArray> importedFavoritesAddrs;
    NodeList importedLoginNodes;         //list of all parent nodes for credentials
    NodeList importedLoginChildNodes;    //list of all parent nodes for credentials
    NodeList importedDataNodes;          //list of all parent nodes for data nodes
    NodeList importedDataChildNodes;     //list of all parent nodes for data nodes
    NodeList importedWebauthnLoginNodes;         //list of all parent nodes for credentials
    NodeList importedWebauthnLoginChildNodes;    //list of all parent nodes for credentials
    QMap<ExportPayloadData, NodeList*> importNodeMap;

    //WebAuthn datas
    NodeList webAuthnLoginNodes;
    NodeList webAuthnLoginNodesClone;
    NodeList webAuthnLoginChildNodes;
    NodeList webAuthnLoginChildNodesClone;


    bool isFw12Flag = false;            // true if fw is at least v1.2

    //this queue is used to put jobs list in a wait
    //queue. it prevents other clients to query something
    //when an AsyncJobs is currently running.
    //An AsyncJobs can also be removed if it was not started (using cancelUserRequest for example)
    //All AsyncJobs does have an <id>
    QQueue<AsyncJobs *> jobsQueue;
    AsyncJobs *currentJobs = nullptr;

    //this is a cache for data upload
    QByteArray currentDataNode;

    //Used to maintain progression for current job
    int progressTotal;
    int progressCurrent;

    FilesCache filesCache;

    bool m_isDebugMsg = false;
    //Message Protocol
    MPDeviceBleImpl *bleImpl = nullptr;
    DeviceSettings *pSettings = nullptr;

protected:
    IMessageProtocol *pMesProt = nullptr;
	bool isBluetooth = false;

    DeviceType deviceType = DeviceType::MOOLTIPASS;
    static constexpr int MP_EXPORT_FIELD_NUM = 10;
    static constexpr int MC_EXPORT_FIELD_NUM = 14;
    static constexpr int BLE_EXPORT_FIELD_MIN_NUM = 18;

    static constexpr int RESET_SEND_DELAY = 300;
    static constexpr int INIT_STARTING_DELAY = RESET_SEND_DELAY + 150;
    static constexpr int STATUS_STARTING_DELAY = RESET_SEND_DELAY + 500;
    static constexpr int CATEGORY_FETCH_DELAY = 5000;
};

#endif // MPDEVICE_H
