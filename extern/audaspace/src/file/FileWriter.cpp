/*******************************************************************************
 * Copyright 2009-2016 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "file/FileWriter.h"
#include "file/FileManager.h"
#include "util/Buffer.h"
#include "IReader.h"
#include "Exception.h"

#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

extern "C"
{ 
	int printf(const char *format, ...); 
}

AUD_NAMESPACE_BEGIN

std::shared_ptr<IWriter> FileWriter::createWriter(std::string filename,DeviceSpecs specs, Container format, Codec codec, unsigned int bitrate)
{
	return FileManager::createWriter(filename, specs, format, codec, bitrate);
}

void FileWriter::writeReader(std::shared_ptr<IReader> reader, std::shared_ptr<IWriter> writer, unsigned int length, unsigned int buffersize, void(*callback)(float, void*), void* data)
{
	Buffer buffer(buffersize * AUD_SAMPLE_SIZE(writer->getSpecs()));
	sample_t* buf = buffer.getBuffer();

	int len;
	bool eos = false;
	int channels = writer->getSpecs().channels;



	//branch mixdown-communication ------------------------------------------------------------------------------------------------------------
	/* socket communication setup BEGIN*/
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	int enable = 1;
	setsockopt(s, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&enable, sizeof(int));
	if (s < 0) {
		std::cout << "Creating Socket Status: FAILED" << std::endl;
	} else {
		std::cout << "Creating Socket Status: SUCCESS" << std::endl;
	}
	struct sockaddr_in address;
	short int port = 1609; //1609 //if 0, will use any available socket.
	memset((char *)&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);//htonl(INADDR_ANY);
	address.sin_port = htons(port);
	/* socket communication setup END*/

	// bind socket to port
	/*int bind_status = bind(s, (struct sockaddr *)&address, sizeof(address));
	if(bind_status < 0) {
		std::cout << "Bind Socket Status: FAILED " << bind_status << std::endl;
	} else {
		std::cout << "Bind Socket Status: SUCCESS" << std::endl;
	}*/

	// get pid as string for packet.
	pid_t pid = getpid();
	std::string proccess_pid = std::to_string( pid );
	//branch mixdown-communication ------------------------------------------------------------------------------------------------------------




	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int i = 0; i < len * channels; i++)
		{
			// clamping!
			if(buf[i] > 1)
				buf[i] = 1;
			else if(buf[i] < -1)
				buf[i] = -1;
		}

		writer->write(len, buf);




		//branch mixdown-communication ------------------------------------------------------------------------------------------------------------
		//print mixdown progress to terminal
		//printf ("MIXDOWN: pos[%d], total[%d]\n", pos + 1, length);
		
		/* socket send message BEGIN */
		//std::string kek = std::to_string(pos) + "." + std::to_string(length) + "." + proccess_pid;
		std::string kek = std::string("{") + 
			"\"position\":" + std::to_string(pos) +
			", \"length\":" + std::to_string(length) + 
			", \"pid\":"    + proccess_pid + 
			"}";
		char message[1024];
		strcpy(message, kek.c_str());
		//std::cout << "Position: " << std::to_string(pos) << ", Length: " << std::to_string(length) << ", PID: " << proccess_pid << std::endl;
		if (sendto(s, message, strlen(message), 0, (struct sockaddr *)&address, sizeof(address)) < 0) {
			//std::cout << "Send Message Status: FAILED" << std::endl;
		} else {
			//std::cout << "Send Message Status: SUCCESS" << std::endl;
		}
		/* socket send message END */
		//branch mixdown-communication ------------------------------------------------------------------------------------------------------------




		if(callback)
		{
			float progress = -1;
			if(length > 0)
				progress = pos / float(length);
			callback(progress, data);
		}
	}

	//branch mixdown-communication ------------------------------------------------------------------------------------------------------------
	//close socket
	close(s);
	//branch mixdown-communication ------------------------------------------------------------------------------------------------------------
}

void FileWriter::writeReader(std::shared_ptr<IReader> reader, std::vector<std::shared_ptr<IWriter> >& writers, unsigned int length, unsigned int buffersize, void(*callback)(float, void*), void* data)
{
	Buffer buffer(buffersize * AUD_SAMPLE_SIZE(reader->getSpecs()));
	Buffer buffer2(buffersize * sizeof(sample_t));
	sample_t* buf = buffer.getBuffer();
	sample_t* buf2 = buffer2.getBuffer();

	int len;
	bool eos = false;
	int channels = reader->getSpecs().channels;

	for(unsigned int pos = 0; ((pos < length) || (length <= 0)) && !eos; pos += len)
	{
		len = buffersize;
		if((len > length - pos) && (length > 0))
			len = length - pos;
		reader->read(len, eos, buf);

		for(int channel = 0; channel < channels; channel++)
		{
			for(int i = 0; i < len; i++)
			{
				// clamping!
				if(buf[i * channels + channel] > 1)
					buf2[i] = 1;
				else if(buf[i * channels + channel] < -1)
					buf2[i] = -1;
				else
					buf2[i] = buf[i * channels + channel];
			}

			writers[channel]->write(len, buf2);
		}

		if(callback)
		{
			float progress = -1;
			if(length > 0)
				progress = pos / float(length);
			callback(progress, data);
		}
	}
}

AUD_NAMESPACE_END
