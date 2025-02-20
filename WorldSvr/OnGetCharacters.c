#include "ClientProtocol.h"
#include "ClientProcedures.h"
#include "ClientSocket.h"
#include "IPCProcedures.h"
#include "Notification.h"
#include "Server.h"

CLIENT_PROCEDURE_BINDING(GET_CHARACTERS) {
    if (!(Client->Flags & CLIENT_FLAGS_VERIFIED) || Client->Account.AccountID < 1) goto error;

    // TODO: Check if is entering premium channel and check service expiration!

    IPC_W2D_DATA_GET_CHARACTER_LIST* Request = IPCPacketBufferInit(Server->IPCSocket->PacketBuffer, W2D, GET_CHARACTER_LIST);
    Request->Header.SourceConnectionID = Connection->ID;
    Request->Header.Source = Server->IPCSocket->NodeID;
    Request->Header.Target.Group = Context->Config.WorldSvr.GroupIndex;
    Request->Header.Target.Type = IPC_TYPE_MASTERDB;
    Request->AccountID = Client->Account.AccountID;
    IPCSocketUnicast(Server->IPCSocket, Request);
    return;

error:
    SocketDisconnect(Socket, Connection);
}

IPC_PROCEDURE_BINDING(D2W, GET_CHARACTER_LIST) {
    if (!ClientConnection || !Client) return;

    memcpy(Client->Characters, Packet->Characters, sizeof(Packet->Characters));
    Client->Flags |= CLIENT_FLAGS_CHARACTER_INDEX_LOADED;
    Client->AccountInfo = Packet->AccountInfo;

    S2C_DATA_GET_CHARACTERS* Response = PacketBufferInit(ClientConnection->PacketBuffer, S2C, GET_CHARACTERS);
    Response->IsSubpasswordSet = strlen(Client->Account.CharacterPassword) > 0;
    Response->Unknown2 = 1;
    Response->AccountInfo = Packet->AccountInfo;

    for (Int32 Index = 0; Index < MAX_CHARACTER_COUNT; Index++) {
        IPC_DATA_CHARACTER_INFO* Character = &Packet->Characters[Index];
        if (Character->ID < 1) continue;

        Character->ID = Character->ID * MAX_CHARACTER_COUNT + Index;
        Character->Style = SwapUInt32(Character->Style);
    }

    memcpy(Response->Characters, Packet->Characters, sizeof(Packet->Characters));
    SocketSend(Context->ClientSocket, ClientConnection, Response);
}
