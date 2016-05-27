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
  //var data = {"time": "1464339114", "power": "1234", "token": "SECRET_TOKEN"};
  
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
  var date = new Date(timeUnix * 1000);
  
  // This allows to get the local time with timezone and DST
  var dateFormatted = Utilities.formatDate(date, TIMEZONE, "yyyy-MM-dd");
  // Make the hours an actual integer
  var hourFormatted = parseInt(Utilities.formatDate(date, TIMEZONE, "H"));
  
  // Define an invalid row number
  var rowId = -1;
  
  // Start from the row after the header (frozen rows)
  var startRow = sheet.getFrozenRows() + 1;
  
  // Check if there is already a row with the given date, skip the top header rows
  var days = sheet.getRange('A' + startRow +':A').getValues();
  
  for(var i in days)
  {
    // If the cell is empty then there is no point to go on
    if(days[i] == "")
    {
      // Stop scanning all the empty rows
      break;
    }
    
    // If the date matches then use that row
    // Use the "yyyy-MM-dd" format as parsing day month and year made it update the wrong day when the day changed for some reason
    // The timezone variable does not matter for a date parsed from "yyyy-MM-dd" format
    var dayFormatted = Utilities.formatDate(new Date(days[i]), 0, "yyyy-MM-dd");
    
    if(dateFormatted == dayFormatted)
    {
      // Skip the header row
      rowId = parseInt(i) + startRow;
      break;
    }
  }
  
  // No row with the same date was found
  if(rowId == -1)
  {
    // Since getLastRow() returns the value of the last non-empty row the value has to be increased manually (+1)
    rowId = sheet.getLastRow() + 1;
  
    // Update the date cell
    sheet.getRange(rowId, columnNames.date).setValue(dateFormatted);
  
    // Update sum formula for the total day power usage
    sheet.getRange(rowId, columnNames.power).setFormula("=SUM(D" + rowId + ":" + rowId + ")");
    
    // If there are at least 7 days of data (minus the header row) the settings of the cells one week ago will be copied over (cost formula and background color)
    if(rowId - 7 > sheet.getFrozenRows())
    {
      // Update the cost formula using 7 rows before it (to get the same weekday) if it is a valid cell
      var cost = sheet.getRange(rowId - 7, columnNames.cost).getFormulaR1C1();
      sheet.getRange(rowId, columnNames.cost).setFormulaR1C1(cost);
      
      // Update the cell formatting (background color) using 7 rows before it
      var backgrounds = sheet.getRange(rowId - 7, columnNames.data, 1, 24).getBackgrounds();
      sheet.getRange(rowId, columnNames.data, 1, 24).setBackgrounds(backgrounds);
    }
  }
  
  // Only fill the cell when there is nothing in it, otherwise it might overwrite important data
  var cell = sheet.getRange(rowId, columnNames.data + hourFormatted);
  if(cell.getValue() == "")
  {
    cell.setValue(power);
  }
}
