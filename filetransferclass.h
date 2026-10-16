#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>

class NetworkTransfer
{
private:

    // Makes sure ALL bytes are sent
    bool sendAll(SOCKET sock, const char* data, int size)
    {
        int total = 0;

        while (total < size)
        {
            int sent = send(
                sock,
                data + total,
                size - total,
                0
            );

            if (sent <= 0)
                return false;

            total += sent;
        }

        return true;
    }


    // Makes sure ALL requested bytes are received
    bool recvAll(SOCKET sock, char* data, int size)
    {
        int total = 0;

        while (total < size)
        {
            int received = recv(
                sock,
                data + total,
                size - total,
                0
            );

            if (received <= 0)
                return false;

            total += received;
        }

        return true;
    }


public:

    // =========================
    // SEND NORMAL MESSAGE
    // =========================

    bool sendMessage(SOCKET sock, const char* message)
    {
        // M = Message
        char type = 'M';

        if (!sendAll(sock, &type, 1))
            return false;


        int length = strlen(message);

        // Send message length
        if (!sendAll(
            sock,
            (char*)&length,
            sizeof(length)
        ))
        {
            return false;
        }


        // Send message itself
        if (!sendAll(
            sock,
            message,
            length
        ))
        {
            return false;
        }


        return true;
    }



    // =========================
    // SEND FILE
    // =========================

    bool sendFile(SOCKET sock, const char* path)
    {
        std::ifstream file(
            path,
            std::ios::binary
        );


        if (!file)
        {
            std::cout << "Could not open file\n";
            return false;
        }


        // -------------------------
        // Get only the filename
        // -------------------------

        std::string filename = path;

        size_t position =
            filename.find_last_of("/\\");

        if (position != std::string::npos)
        {
            filename =
                filename.substr(position + 1);
        }


        // -------------------------
        // Find file size
        // -------------------------

        file.seekg(0, std::ios::end);

        long long fileSize =
            (long long)file.tellg();

        file.seekg(0, std::ios::beg);


        // -------------------------
        // Tell receiver:
        // FILE IS COMING
        // -------------------------

        char type = 'F';

        if (!sendAll(sock, &type, 1))
            return false;


        // -------------------------
        // Send filename
        // -------------------------

        int filenameLength =
            (int)filename.length();


        if (!sendAll(
            sock,
            (char*)&filenameLength,
            sizeof(filenameLength)
        ))
        {
            return false;
        }


        if (!sendAll(
            sock,
            filename.c_str(),
            filenameLength
        ))
        {
            return false;
        }


        // -------------------------
        // Send filesize
        // -------------------------

        if (!sendAll(
            sock,
            (char*)&fileSize,
            sizeof(fileSize)
        ))
        {
            return false;
        }


        // -------------------------
        // Send actual file
        // -------------------------

        char buffer[4096];


        while (
            file.read(buffer, sizeof(buffer))
            ||
            file.gcount() > 0
            )
        {
            int bytesRead =
                (int)file.gcount();


            if (!sendAll(
                sock,
                buffer,
                bytesRead
            ))
            {
                return false;
            }
        }


        file.close();

        std::cout << "File sent!\n";

        return true;
    }



    // =========================
    // RECEIVE MESSAGE OR FILE
    // =========================

    char receive(
        SOCKET sock,
        std::string& result
    )
    {
        char type;


        // First receive:
        // M or F

        if (!recvAll(sock, &type, 1))
        {
            return 0;
        }



        // =========================
        // MESSAGE
        // =========================

        if (type == 'M')
        {
            int length;


            if (!recvAll(
                sock,
                (char*)&length,
                sizeof(length)
            ))
            {
                return 0;
            }


            if (length < 0 || length > 1000000)
                return 0;


            result.resize(length);


            if (length > 0)
            {
                if (!recvAll(
                    sock,
                    &result[0],
                    length
                ))
                {
                    return 0;
                }
            }


            return 'M';
        }



        // =========================
        // FILE
        // =========================

        if (type == 'F')
        {
            int filenameLength;


            if (!recvAll(
                sock,
                (char*)&filenameLength,
                sizeof(filenameLength)
            ))
            {
                return 0;
            }


            if (
                filenameLength <= 0 ||
                filenameLength > 255
                )
            {
                return 0;
            }


            std::string filename;

            filename.resize(filenameLength);


            if (!recvAll(
                sock,
                &filename[0],
                filenameLength
            ))
            {
                return 0;
            }


            long long fileSize;


            if (!recvAll(
                sock,
                (char*)&fileSize,
                sizeof(fileSize)
            ))
            {
                return 0;
            }


            if (fileSize < 0)
                return 0;


            // Save as:
            // received_cat.jpg

            std::string saveName =
                "received_" + filename;


            std::ofstream file(
                saveName,
                std::ios::binary
            );


            if (!file)
            {
                std::cout
                    << "Could not create file\n";

                return 0;
            }


            char buffer[4096];

            long long totalReceived = 0;


            while (totalReceived < fileSize)
            {
                long long remaining =
                    fileSize - totalReceived;


                int amount = sizeof(buffer);


                if (remaining < amount)
                {
                    amount = (int)remaining;
                }


                int bytes = recv(
                    sock,
                    buffer,
                    amount,
                    0
                );


                if (bytes <= 0)
                {
                    file.close();
                    return 0;
                }


                file.write(
                    buffer,
                    bytes
                );


                totalReceived += bytes;
            }


            file.close();


            result = saveName;


            return 'F';
        }


        return 0;
    }
};