/*
  IoTPowerMeter - Software for the IoTPowerMeter based on the ESP8266

  Copyright (c) 2016 Karl Kangur. All rights reserved.
  This file is part of IoTPowerMeter.

  IoTPowerMeter is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  IoTPowerMeter is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with IoTPowerMeter.  If not, see <http://www.gnu.org/licenses/>.

*/

/*
 * File:    server.cpp
 * Author:  Karl Kangur <karl.kangur@gmail.com>
 * Licnece: GPL
 * URL:     https://github.com/Nurgak/Internet-of-Things-Power-Meter
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <TimeLib.h>
#include <SPI.h>
#include <SD.h>

#include "config.h"
#include "server.h"
#include "IoTPowerMeter.h"

ESP8266WebServer server(80);
File uploadFile;

void initServer()
{
  server.on("/", [](){ loadFromSdCard("/"); }); // Serve the index.htm file for root
  server.on("/list", HTTP_GET, printDirectory); // Fetch files in directory
  server.on("/edit", HTTP_DELETE, handleDelete); // Delete files and folders
  server.on("/edit", HTTP_PUT, handleCreate); // For uploads
  
  server.on("/api", HTTP_GET, serverApi);
  server.on("/edit", HTTP_POST, [](){ returnOK(); }, handleFileUpload);

  // In case there is no handler try to serve a page from SD card
  server.onNotFound(handleNotFound);
  
  server.begin();
}

void handleClient()
{
  server.handleClient();
}

// Authentificate the user for server access
// Can be used in the form of http://username:password@power.local
boolean basicAuthentication()
{
  DEBUGV("User authertification request\n");
  
  if(!server.authenticate(http_username, http_password))
  {
    DEBUGV("Unauthorised action\n");
    
    // This also sends the authentication request to the client
    server.requestAuthentication();

    // Inform that the request should not proceed
    return false;
  }
  return true;
}

// Application Programming Interface for the IoT Power Meter
void serverApi()
{
  if(!basicAuthentication())
  {
    return;
  }
  
  server.sendHeader(F("Connection"), F("close"));

  // An API request must have a "request" parameter
  if(!server.hasArg("request"))
  {
    server.send(400, F("text/plain"), F("Bad argument"));
    return;
  }
  
  DEBUGV("API request: %s\n", (char *)server.arg("request").c_str());
  
  if(server.arg("request") == "live")
  {
    // Live power usage in [Wh]
    server.send(200, F("text/plain"), (String)livePowerUsage());
  }
  else if(server.arg("request") == "today")
  {
    // Return today electricity usage so far [Wh]
    server.send(200, F("text/plain"), (String)todayPowerUsage());
  }
  else if(server.arg("request") == "values")
  {
    // Send all todays values to build a graph
    char buffer[32];
    sprintf(buffer, "/power/%04d%02d%02d.csv", year(), month(), day());
    // Stream todays file
    loadFromSdCard(buffer);
  }
  else
  {
    server.send(400, F("text/plain"), F("Bad argument"));
  }
}

void returnOK()
{
  server.sendHeader(F("Connection"), F("close"));
  server.send(200, F("text/plain"), F("OK"));
}

void returnFail(String msg)
{
  server.sendHeader(F("Connection"), F("close"));
  server.send(500, F("text/plain"), msg + F("\r\n"));
}

bool loadFromSdCard(String path)
{
  if(!basicAuthentication())
  {
    return false;
  }
  
  // Default data return type is plain text
  String dataType = F("text/plain");

  // In case the user requests / or /*/ load the index.htm file by default
  if(path.endsWith("/"))
  {
    path += "index.htm";
  }

  // Define the data type of the returned data from the file extention
  if(path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }
  else if(path.endsWith(".htm"))
  {
    dataType = F("text/html");
  }
  else if(path.endsWith(".css"))
  {
    dataType = F("text/css");
  }
  else if(path.endsWith(".js"))
  {
    dataType = F("application/javascript");
  }
  else if(path.endsWith(".png"))
  {
    dataType = F("image/png");
  }
  else if(path.endsWith(".gif"))
  {
    dataType = F("image/gif");
  }
  else if(path.endsWith(".jpg"))
  {
    dataType = F("image/jpeg");
  }
  else if(path.endsWith(".ico"))
  {
    dataType = F("image/x-icon");
  }
  /*else if(path.endsWith(".xml"))
  {
    dataType = F("text/xml");
  }
  else if(path.endsWith(".pdf"))
  {
    dataType = F("application/pdf");
  }
  else if(path.endsWith(".zip"))
  {
    dataType = F("application/zip");
  }*/

  // Try to find the file on the SD card
  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory())
  {
    path += F("/index.htm");
    dataType = F("text/html");
    dataFile = SD.open(path.c_str());
  }

  // File not found on the SD card
  if(!dataFile)
  {
    return false;
  }

  // Request was to download the file, so stream it
  if(server.hasArg("download"))
  {
    dataType = F("application/octet-stream");
  }
  
  DEBUGV("Streaming file: %s\n", (char *)path.c_str());

  // Actually send the file, check that all data was sent
  if(server.streamFile(dataFile, dataType) != dataFile.size())
  {
    DEBUGV("Sent less data than expected!\n");
  }

  dataFile.close();
  return true;
}

void handleFileUpload()
{
  if(!basicAuthentication())
  {
    return;
  }

  // Upload requests are only allowed from the /edit page
  if(server.uri() != "/edit")
  {
    return;
  }
  
  HTTPUpload& upload = server.upload();
  
  if(upload.status == UPLOAD_FILE_START)
  {
    // Overwrite existing file
    if(SD.exists((char *)upload.filename.c_str()))
    {
      // Remove the existing file first
      SD.remove((char *)upload.filename.c_str());
    }
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DEBUGV("Upload: START, filename: %s\n", (char *)upload.filename.c_str());
  }
  else if(upload.status == UPLOAD_FILE_WRITE)
  {
    if(uploadFile)
    {
      uploadFile.write(upload.buf, upload.currentSize);
    }
    DEBUGV("Upload: WRITE, Bytes: %d\n", upload.currentSize);
  }
  else if(upload.status == UPLOAD_FILE_END)
  {
    if(uploadFile)
    {
      uploadFile.close();
    }
    DEBUGV("Upload: END, Size: %d\n", upload.totalSize);
  }
}

void deleteRecursive(String path)
{
  // This function is only called internally and does not need authentication
  
  File file = SD.open((char *)path.c_str());
  
  DEBUGV("Deleting: %s\n", (char *)path.c_str());
  
  if(!file.isDirectory())
  {
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  
  while(true)
  {
    File entry = file.openNextFile();
    if(!entry)
    {
      break;
    }
    String entryPath = path + "/" + entry.name();
    
    if(entry.isDirectory())
    {
      entry.close();
      deleteRecursive(entryPath);
    }
    else
    {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete()
{
  if(!basicAuthentication())
  {
    return;
  }
  
  if(server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  String path = server.arg(0);
  
  if(path == "/" || !SD.exists((char *)path.c_str()))
  {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate()
{
  if(!basicAuthentication())
  {
    return;
  }
  
  if(server.args() == 0)
  {
    return returnFail("BAD ARGS");
  }
  
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str()))
  { 
    returnFail("BAD PATH");
    return;
  }
  
  DEBUGV("Creating: %s\n", (char *)path.c_str());

  if(path.indexOf('.') > 0)
  {
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file)
    {
      file.write((const char *)0);
      file.close();
    }
  }
  else
  {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory()
{
  if(!basicAuthentication())
  {
    return;
  }
  
  if(!server.hasArg("dir"))
  {
    return returnFail("BAD ARGS");
  }
  
  String path = server.arg("dir");
  
  DEBUGV("Fetching list for: %s\n", (char *)path.c_str());
  
  if(path != "/" && !SD.exists((char *)path.c_str()))
  {
    return returnFail("BAD PATH");
  }
  
  File dir = SD.open((char *)path.c_str());
  
  path = String();
  if(!dir.isDirectory())
  {
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("text/json"), "");
  
  server.sendContent("[");
  for(unsigned int cnt = 0; true; ++cnt)
  {
    File entry = dir.openNextFile();
    if(!entry)
    {
      break;
    }

    String output;
    if(cnt != 0)
    {
      output = ',';
    }

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
}

void handleNotFound()
{
  // Serve file from SD card if it has been found
  if(loadFromSdCard(server.uri()))
  {
    return;
  }

  // loadFromSdCard() already does authentication
  
  DEBUGV("File not found: %s\n", (char *)server.uri().c_str());

  // Otherwise reply with a standard 404 error
  server.send(404, F("text/plain"), F("Not found"));
}

