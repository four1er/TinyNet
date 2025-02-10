#include "../src/all.h"
#include "../src/promises.h"

#include <signal.h>

using NNet::TAddress;
using NNet::TEpoll;
using NNet::TSocket;
using NNet::TVoidTask;
using TLoop = NNet::TLoop<TEpoll>;

TVoidTask client_handler(TSocket socket, TLoop* loop) {
    char buffer[128] = {0};
    ssize_t size = 0;

    try {
        while ((size = co_await socket.ReadSome(buffer, sizeof(buffer))) > 0) {
            std::cerr << "Received from client: '" << std::string_view(buffer, size) << "' ("
                      << size << ") bytes \n";
            co_await socket.WriteSome(buffer, size);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << "\n";
    }
    if (size == 0) {
        std::cerr << "Client disconnected\n";
    }
    co_return;
}

TVoidTask server(TLoop* loop) {
    TAddress addr{"127.0.0.1", 8888};
    TSocket socket(loop->Poller(), addr.Domain());
    socket.Bind(addr);
    socket.Listen();
    std::cout << "Server started Listen\n";

    try {
        while (true) {
            auto client = co_await socket.Accept();
            std::cout << "wait for client" << std::endl;
            client_handler(std::move(client), loop);
            std::cout << "client disconnected\n";
        }
    } catch (const std::exception& ex) {
        std::cout << "Exception: " << ex.what() << "\n";
    }
    co_return;
}

TVoidTask client(TLoop* loop, int clientId) {
    char buffer[128] = "Hello XXX/YYY";
    char rcv[128] = {0};
    int messageNo = 1;
    ssize_t size = 0;

    std::cout << "create client, id: " << clientId << std::endl;

    try {
        TAddress addr{"127.0.0.1", 8888};
        TSocket socket(loop->Poller(), addr.Domain());
        co_await socket.Connect(addr);

        do {
            snprintf(buffer + 6, sizeof(buffer) - 6, "%03d/%03d", messageNo++, clientId);
            size = co_await socket.WriteSome(buffer, sizeof(buffer));
            size = co_await socket.ReadSome(rcv, sizeof(rcv));
            std::cerr << "Received from server: " << std::string_view(rcv, size) << " (" << size
                      << ") bytes \n";
            co_await loop->Poller().Sleep(std::chrono::milliseconds(1000));
        } while (size > 0);
    } catch (const std::exception& ex) {
        std::cout << "Exception: " << ex.what() << "\n";
    }
    co_return;
}

int main(int argc, char** argv) {
    TLoop loop;
    int clients = 0;
    if (argc > 1) {
        clients = atoi(argv[1]);
    }
    if (clients == 0) {
        clients = 1;
    }

    server(&loop);
    for (int i = 0; i < clients; i++) {
        client(&loop, i + 1);
    }
    std::cout << "Start loop..." << std::endl;
    loop.Loop();
    return 0;
}
