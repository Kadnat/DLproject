// Mimic an Enum for Severity Levels
const Severity = {
  INFO: 'INFO',
  WARNING: 'WARNING',
  ERROR: 'ERROR',
  DEBUG: 'DEBUG'
};

/**
 * Class representing a logger with functionality to log messages to a Google Spreadsheet.
 */
class LocalLogger {
  /**
   * Creates a logger instance.
   * @param {string} emailAddress - The email address to send notifications to. (Leave blank to disable notifications)
   * @param {boolean} logAtTop - Determines if logs should be placed at the top of the spreadsheet.
   * @param {GoogleAppsScript.Spreadsheet.Sheet} [logSheet] - Optional. The sheet to log messages to.
   */
  constructor(emailAddress = null, logAtTop = false, notifyLevel = Severity.ERROR, logSheet = null) {
    this.spreadsheet = SpreadsheetApp.openById('SheetID');
    this.logSheet = logSheet || this.spreadsheet.getSheetByName('Logs');
    this.logAtTop = logAtTop;
    this.notifyLevel = notifyLevel; // Notify when an ERROR or more severe log is added
    this.emailAddress = emailAddress; // Email address for sending notifications
  }

  /**
   * Initializes the logger by creating a Logs sheet if it does not exist.
   */
  init() {
    if (!this.logSheet) {
      this.logSheet = this.spreadsheet.insertSheet('Logs');
      this.logSheet.appendRow(['Timestamp', 'Severity', 'Message', 'User/Session']);
      this.logSheet.getRange('1:1').setFontWeight('bold');
    }
  }

  /**
   * Logs a message to the spreadsheet.
   * @param {string} message - The message to log.
   * @param {string} [severity=Severity.INFO] - The severity of the log message.
   */
  log(message, severity = Severity.INFO) {
    const userEmail = Session.getActiveUser().getEmail(); // May return empty in some contexts due to privacy
    const sessionID = Session.getTemporaryActiveUserKey(); // Alternative session identifier
    const userInfo = userEmail || sessionID;
    const timestamp = new Date();
    const formattedTimestamp = Utilities.formatDate(timestamp, Session.getScriptTimeZone(), "yyyy-MM-dd HH:mm:ss");
    const logEntry = [formattedTimestamp, severity, message, userInfo];

    try {
      this.writeLogEntry(logEntry);

      // Trigger notification if severity is high
      if (this.shouldNotify(severity)) {
        this.notify(message, severity);
      }
    } catch (e) {
      // Handle the error, e.g., log to a different place or send an email
      Logger.log('Failed to log message: ' + e.toString());
    }
  }

  /**
   * Writes a single log entry to the spreadsheet.
   * @param {Array} entry - The log entry to write.
   */
  writeLogEntry(entry) {
    let range;
    if (this.logAtTop) {
      // Insert a new row after the headers for the new log entry
      this.logSheet.insertRowAfter(1);
      range = this.logSheet.getRange(2, 1, 1, 4); // Now the new entry will be on the second row
    } else {
      // Append at the bottom of the log sheet
      const lastRow = this.logSheet.getLastRow();
      range = this.logSheet.getRange(lastRow + 1, 1, 1, 4);
    }
    range.setValues([entry]);
    range.setFontWeight('normal');
    this.applyLogColor(range, entry[1]); // Apply color based on severity
  }

  /**
   * Sends a notification for a log entry.
   * @param {string} message - The log message.
   * @param {string} severity - The severity of the log message.
   */
  notify(message, severity) {
    if (!this.emailAddress) {
      Logger.log('No email address specified for notifications');
      return;
    }

    try {
      const subject = `New ${severity} log entry`;
      const body = `A new log entry with severity ${severity} was added: \n ${message}`;
      MailApp.sendEmail(this.emailAddress, subject, body); // Use the stored email address
    } catch (e) {
      Logger.log('Failed to send notification: ' + e.toString());
    }
  }

  /**
   * Determines if a log entry should trigger a notification based on its severity.
   * @param {string} severity - The severity of the log entry.
   * @return {boolean} True if notification should be sent, false otherwise.
   */
  shouldNotify(severity) {
    const severityOrder = [Severity.DEBUG, Severity.INFO, Severity.WARNING, Severity.ERROR];
    return severityOrder.indexOf(severity) >= severityOrder.indexOf(this.notifyLevel);
  }

  /**
   * Applies background color to a log entry based on its severity.
   * @param {GoogleAppsScript.Spreadsheet.Range} range - The range to apply the background color to.
   * @param {string} severity - The severity of the log entry.
   */
  applyLogColor(range, severity) {
    let color = "#FFFFFF"; // Default white background
    switch (severity) {
      case Severity.INFO:
        color = "#D9EAD3"; // Light green
        break;
      case Severity.WARNING:
        color = "#FFE599"; // Light yellow
        break;
      case Severity.ERROR:
        color = "#F4CCCC"; // Light red
        break;
      case Severity.DEBUG:
        color = "#CFE2F3"; // Light blue
        break;
    }
    range.setBackground(color);
  }
}

function doGet(e) {
  var spreadsheet = SpreadsheetApp.openById('SheetID');
  var params = e.parameter;

  // Create an instance of LocalLogger
  var logger = new LocalLogger('test@gmail.com', false, Severity.ERROR);
  logger.init();

  // Log the entire parameters object for debugging
  var logMessage = 'Params: ' + JSON.stringify(params);
  logger.log(logMessage, Severity.DEBUG);

  var node = params['node']; // Extract the node number from the URL parameters

  // Log the extracted node value
  logMessage = 'Node: ' + node;
  logger.log(logMessage, Severity.INFO);

  if (!node) {
    logMessage = 'Error: Node parameter is missing.';
    logger.log(logMessage, Severity.ERROR);
    return ContentService.createTextOutput('Error: Node parameter is missing.');
  }

  var sheetName = node.toString(); // Use the node number as the sheet name
  var sheet = spreadsheet.getSheetByName(sheetName);

  if (!sheet) {
    // If the sheet doesn't exist, create it and add the headers
    sheet = spreadsheet.insertSheet(sheetName);
    var headers = Object.keys(params);

    // Log the headers for debugging
    logMessage = 'Headers: ' + headers;
    logger.log(logMessage, Severity.DEBUG);

    sheet.appendRow(headers);
  }

  // Add the data to the sheet
  var newRow = [];
  for (var i = 0; i < sheet.getLastColumn(); i++) {
    var header = sheet.getRange(1, i + 1).getValue();

    // Log each header and corresponding value for debugging
    logMessage = 'Header: ' + header + ', Value: ' + params[header];
    logger.log(logMessage, Severity.DEBUG);

    newRow.push(params[header]);
  }

  sheet.appendRow(newRow);

  return ContentService.createTextOutput('Success');
}

// Function to test doGet with specific parameters
function testDoGet() {
  var e = {
    parameter: {
      datetime: '2024-6-5 6:7:13',
      node: '2',
      temp: '23.2429',
      hum: '48.5815',
      press: '987.4500',
      dendro: '1724.3000',
      moisture: '0.0000'
    }
  };

  var result = doGet(e);
  Logger.log(result.getContent());
}

