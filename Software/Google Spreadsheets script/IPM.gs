// Spreadsheet that is holding all the data
var SPREADSHEET_ID = "SPREADSHEET_ID";
var TIMEZONE = "Europe/Zurich";
// Daylight savings time is not applied when this form is used:
//var TIMEZONE = "GMT+1";
// Secret token to make sure nobody else can submit data, even if the URL is disclosed
var TOKEN = "SECRET_TOKEN";

function doGet(e)
{
  return handleResponse(e);
}

function doPost(e)
{
  return handleResponse(e);
}

function handleResponse(e)
{  
  // Extract the GET paramaters
  var data = e.parameter;
  // Dummy data for testing
  //var data = {"time": "1464077423", "power": "5555", "token": "SECRET_TOKEN"};
  
  // Make sure all data is passed
  if(!data.time || !data.power || !data.token)
  {
    return ContentService.createTextOutput("Missing data");
  }
  
  // Make sure the data is valid, only numbers for time and power and only characters [a-zA-Z_0-9] for the token
  if(!data.time.match(/^\d+$/) || !data.power.match(/^\d+$/) || !data.token.match(/^\w+$/))
  {
    return ContentService.createTextOutput("Incorrect data");
  }
  
  // Make sure the token corresponds
  if(data.token != TOKEN)
  {
    return ContentService.createTextOutput("Wrong token");
  }
  
  var spreadsheet = getSpreadsheet(SPREADSHEET_ID);
  if(!spreadsheet)
  {
    return ContentService.createTextOutput("Spreadsheet not found");
  }
  
  // Open the day sheet
  var sheetDay = getSheet(spreadsheet, "Day");
  if(!sheetDay)
  {
    return ContentService.createTextOutput("Sheet not found");
  }
  
  insertData(sheetDay, data.time, data.power);
  
  return HtmlService.createHtmlOutput("OK");
}

function getSpreadsheet(id)
{
  // Open the spreadsheet
  var spreadsheet = SpreadsheetApp.openById(id);
  
  // Check that we got a spreadsheet
  if(!spreadsheet)
  {
    return false;
  }
  
  return spreadsheet;
}

function getSheet(spreadsheet, name)
{
  // Open the sheet
  var sheetDay = spreadsheet.getSheetByName(name);
  
  // Check that we got a sheet
  if(!sheetDay)
  {
    return false;
  }
  
  return sheetDay;
}

function insertData(sheet, timeUnix, power)
{  
  // Column IDs
  var columnNames = {"date": 1, "power": 2, "cost": 3, "data": 4};
  
  // Time is passed in UNIX timestamp format in seconds, JavaScript wants milliseconds
  var time = new Date(timeUnix * 1000);
  //var time = new Date((parseInt(timeUnix) + TIMEZONE * 3600) * 1000);
  
  // Define an invalid row number
  var rowId = -1;
  
  // Check if there is already a row with the given date
  var days = sheet.getRange('A:A').getValues();
  
  for(var i in days)
  {
    // If the cell is empty then there is no point to go on
    if(days[i] == "")
    {
      // Stop scanning all the empty rows
      break;
    }
    
    // Parse the date in the row
    var date = new Date(days[i]);
    var year = date.getFullYear();
    var month = date.getMonth();
    var day = date.getDate();
    
    // If the date matches then use that row
    if(time.getFullYear() == year && time.getMonth() == month && time.getDate() == day)
    {
      // Add 1 to skip the header row
      rowId = parseInt(i) + 1;
      break;
    }
  }
  
  // No row with the same date was found
  if(rowId == -1)
  {
    // Since getLastRow() returns the value of the last non-empty row the value has to be increased manually (+1)
    rowId = sheet.getLastRow() + 1;
  
    // Update the date cell
    var formattedDate = Utilities.formatDate(time, TIMEZONE, "yyyy-MM-dd");
    sheet.getRange(rowId, columnNames.date).setValue(formattedDate);
  
    // Update sum formula for the total day power usage
    sheet.getRange(rowId, columnNames.power).setFormula("=SUM(D"+rowId+":"+rowId+")");
    
    // If there are at least 7 days of data (minus the header row) the settings of the cells one week ago will be copied over (cost formula and background color)
    if(rowId - 7 > 1)
    {
      // Update the cost formula using 7 rows before it (to get the same weekday) if it is a valid cell
      var cost = sheet.getRange(rowId - 7, columnNames.cost).getFormulaR1C1();
      sheet.getRange(rowId, columnNames.cost).setFormulaR1C1(cost);
      
      // Update the cell formatting (background color) using 7 rows before it
      var backgrounds = sheet.getRange(rowId - 7, columnNames.data, 1, 24).getBackgrounds();
      sheet.getRange(rowId, columnNames.data, 1, 24).setBackgrounds(backgrounds);
    }
  }
  
  // Set the power for the hour, the formatDate() allows to get the local time with timezone and DST
  var hour = parseInt(Utilities.formatDate(time, TIMEZONE, "H"));
  // Only fill the cell when there is nothing in it, otherwise it might overwrite important data
  var cell = sheet.getRange(rowId, columnNames.data + hour);
  if(cell.getValue() == "")
  {
    cell.setValue(power);
  }
}
