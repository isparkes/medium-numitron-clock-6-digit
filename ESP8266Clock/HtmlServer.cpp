#include "HtmlServer.h"

ESP8266WebServer server(80);            // port 80
ESP8266HTTPUpdateServer httpUpdater;    // ESP8266HTTPUpdateServer.h
MDNSResponder mdns;

// ----------------------- Auth ------------------------

boolean webAuthentication = WEB_AUTH_DEFAULT;
String webUsername = WEB_USERNAME_DEFAULT;
String webPassword = WEB_PASSWORD_DEFAULT;

// ************************************************************
// Check server args for a Boolean value and note if it has changed
// ************************************************************
void checkServerArgBoolean(String argument, String argumentName, String trueLiteral, String falseLiteral, boolean &changed, boolean &value) {
  if (server.hasArg(argument)) {
    debugMsg("Got " + argumentName + " : " + server.arg(argument));
    if ((server.arg(argument) == falseLiteral) && (value)) {
      debugMsg("-> Set " + falseLiteral + " mode");
      changed = true;
      value = false;
    }

    if ((server.arg(argument) == trueLiteral) && (!value)) {
      debugMsg("-> Set " + trueLiteral + " mode");
      changed = true;
      value = true;
    }
  }
}

// ************************************************************
// Check server args for an int value and note if it has changed
// ************************************************************
void checkServerArgInt(String argument, String argumentName, boolean &changed, int &value) {
  if (server.hasArg(argument)) {
    debugMsg("Got " + argumentName + " : " + server.arg(argument));
    int newValue = atoi(server.arg(argument).c_str());
    if (newValue != value) {
      changed = true;
      value = newValue;
      debugMsg("-> Set " + argumentName + ": " + String(value));
    }
  }
}

// ************************************************************
// Check server args for a byte value and note if it has changed
// ************************************************************
void checkServerArgByte(String argument, String argumentName, boolean &changed, byte &value) {
  if (server.hasArg(argument)) {
    debugMsg("Got " + argumentName + " : " + server.arg(argument));
    byte newValue = atoi(server.arg(argument).c_str());
    if (newValue != value) {
      changed = true;
      value = newValue;
      debugMsg("-> Set " + argumentName + ": " + String(value));
    }
  }
}

// ************************************************************
// Check server args for a string value and note if it has changed
// ************************************************************
void checkServerArgString(String argument, String argumentName, boolean &changed, String &value) {
  if (server.hasArg(argument)) {
    debugMsg("Got " + argumentName + " : " + server.arg(argument));
    String newValue = server.arg(argument);
    if (value != newValue) {
      value = newValue;
      debugMsg("-> Set " + argumentName + " = " + value);
      changed = true;
    }
  }
}

boolean getWebAuthentication() {
  return webAuthentication;
}

void setWebAuthentication(boolean newValue) {
  webAuthentication = newValue;
}

String getWebUserName() {
  return webUsername;
}

void setWebUserName(String newValue) {
  webUsername = newValue;
}

String getWebPassword() {
  return webPassword;
}

void setWebPassword(String newValue) {
  webPassword = newValue;
}

// ----------------------------------------------------------------------------------------------------
// ------------------------------------------- HTML functions -----------------------------------------
// ----------------------------------------------------------------------------------------------------

String getHTMLHead(boolean isConnected) {
  String header = "<!DOCTYPE html><html><head>";

  if (isConnected) {
    header += "<link href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css\" rel=\"stylesheet\" integrity=\"sha384-1q8mTJOASx8j1Au+a5WDVnPi2lkFfwwEAa8hDDdjZlpLegxhjVME1fgjWPGmkzs7\" crossorigin=\"anonymous\">";
    header += "<link href=\"http://www.open-rate.com/wl.css\" rel=\"stylesheet\" type=\"text/css\">";
    header += "<script src=\"http://code.jquery.com/jquery-1.12.3.min.js\" integrity=\"sha256-aaODHAgvwQW1bFOGXMeX+pC4PZIPsvn2h1sArYOhgXQ=\" crossorigin=\"anonymous\"></script>";
    header += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js\" integrity=\"sha384-0mSbJDEHialfmuBBQP6A4Qrprq5OVfW37PRR3j5ELqxss1yVqOtnepnHVP9aJ7xS\" crossorigin=\"anonymous\"></script>";
  } else {
    header += "<link href=\"local.css\" rel=\"stylesheet\">";
  }
  header += "<title>";
  header += HTML_TITLE;
  header += "</title></head>";
  header += "<body>";
  return header;
}

/**
   Get the bootstrap top row navbar, including the Bootstrap links
*/
String getNavBar() {
  String navbar = "<nav class=\"navbar navbar-inverse navbar-fixed-top\">";
  navbar += "<div class=\"container-fluid\"><div class=\"navbar-header\"><button type=\"button\" class=\"navbar-toggle collapsed\" data-toggle=\"collapse\" data-target=\"#navbar\" aria-expanded=\"false\" aria-controls=\"navbar\">";
  navbar += "<span class=\"sr-only\">Toggle navigation</span><span class=\"icon-bar\"></span><span class=\"icon-bar\"></span><span class=\"icon-bar\"></span></button>";
  navbar += "<a class=\"navbar-brand\" href=\"https://www.nixieclock.biz\">ESP Numitron Clock</a></div><div id=\"navbar\" class=\"navbar-collapse collapse\"><ul class=\"nav navbar-nav navbar-right\">";
  navbar += "<li><a href=\"/\">Summary</a></li><li><a href=\"/alarms\">Strikes and Alarms</a></li><li><a href=\"/time\">Configure Time Server</a></li><li><a href=\"/clockconfig\">Configure clock settings</a></li><li><a href=\"/utility\">Utility</a></li></ul></div></div></nav>";
  return navbar;
} 


String getExplanationText(String textToprint) {
  return "<span class=\"form-group\">" + textToprint + "</span>";
}

/**
   Get the header for a 2 column table
*/
String getTableHead2Col(String tableHeader, String col1Header, String col2Header) {
  String tableHead = "<div class=\"container\" role=\"main\"><h3 class=\"sub-header\">";
  tableHead += tableHeader;
  tableHead += "</h3><div class=\"table-responsive\"><table class=\"table table-striped\"><thead><tr><th class=\"col-xs-6\">";
  tableHead += col1Header;
  tableHead += "</th><th class=\"col-xs-6\">";
  tableHead += col2Header;
  tableHead += "</th></tr></thead><tbody>";

  return tableHead;
}

String getTableRow2Col(String col1Val, String col2Val) {
  String tableRow = "<tr><td>";
  tableRow += col1Val;
  tableRow += "</td><td>";
  tableRow += col2Val;
  tableRow += "</td></tr>";

  return tableRow;
}

String getTableRow2Col(String col1Val, int col2Val) {
  String tableRow = "<tr><td>";
  tableRow += col1Val;
  tableRow += "</td><td>";
  tableRow += col2Val;
  tableRow += "</td></tr>";

  return tableRow;
}

String getTableFoot() {
  return "</tbody></table></div></div>";
}

/**
   Get the header for an event table
*/
String getTableHeadEvent(String tableHeader, String col1Header, String col2Header,
			 String col3Header, String col4Header, String col5Header,
			 String col6Header, String col7Header, String col8Header,
			 String col9Header, String col10Header, String col11Header) {

  String tableHead = "<div class=\"container\" role=\"main\"><h3 class=\"sub-header\">";
  tableHead += tableHeader;
  tableHead += "</h3><div class=\"table-responsive\"><table class=\"table table-striped\"><thead><tr><th class=\"col-xs-1\">";
  tableHead += col1Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col2Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col3Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col4Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col5Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col6Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col7Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col8Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col9Header;
  tableHead += "</th><th class=\"col-xs-1\">";
  tableHead += col10Header;
  tableHead += "</th><th class=\"col-xs-2\">";
  tableHead += col11Header;
  tableHead += "</th></tr></thead><tbody>";

  return tableHead;
}

String getTableRowEvent(strikeEvent *s) {
  String tableRow = "\n<form> <tr><td>";
  tableRow += getEventIntTimeSelection("year", year(), year() + 15,
				       tmYearToCalendar(s->Event.Year),
				       s->Event.Year == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("month", 1, 12, s->Event.Month,
				      s->Event.Month == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("day", 0, 31, s->Event.Day,
				      s->Event.Day == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("wday", 1, 7, s->Event.Wday,
				      s->Event.Wday == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("hour", 0, 23, s->Event.Hour,
				      s->Event.Hour == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("minute", 0, 59, s->Event.Minute,
				      s->Event.Minute == WILD);
  tableRow += "</td><td>";
  tableRow += getEventIntTimeSelection("second", 0, 59, s->Event.Second,
				      s->Event.Second == WILD);
  tableRow += "</td><td>";
  String strikeOpts[4] = {"Single", "Hours", "Ship's Bells", "Fibonacchi"};
  tableRow += getEventOptsSelection("strike", strikeOpts, 4, s->strikePattern);
  tableRow += "</td><td>";
  String repeatOpts[2] = {"Yes", "No"};
  tableRow += getEventOptsSelection("repeat", repeatOpts, 2, s->eventRepeat);
  tableRow += "</td><td>";
  String activeOpts[2] = {"Yes", "No"};
  tableRow += getEventOptsSelection("active", activeOpts, 2, s->eventActive);
  tableRow += "</td><td>";
  tableRow += getEventSubmitButtons("Set", "Del");
  tableRow += "\n<input type=\"hidden\" name=\"eventKey\" value=\"";
  tableRow += (unsigned long) s;
  tableRow += "\">";
  tableRow += "</td></tr></form>\n";

  return tableRow;
}

String getEventOptsSelection(String name, String opts[], int nopts, byte selected) {
  String result = "\n";
  result += "<select class=\"form-control\" name=\"";
  result += name;
  result += "\">";
  for (int i = 1; i <= nopts; i++) {
    result += "<option ";
    if (i == selected) {
      result += "selected ";
    }
    result += "value =\"";
    result += i;
    result += "\">";
    result += opts[i-1];
    result += "</option>";
  }
  result += "</select>";
  return result;
}

String getEventIntTimeSelection(String name, int low, int high, int selected,
				boolean wild) {
  String result = "\n";
  result += "<select class=\"form-control\" name=\"";
  result += name;
  result += "\">";
  result += "<option ";
  if (wild)
    result += "selected ";
  result += "value =\"*\">*</option>";
  for (int i = low; i <= high; i++) {
    result += "<option ";
    if (i == selected && !wild) {
      result += "selected ";
    }
    result += "value =\"";
    result += i;
    result += "\">";
    result += i;
    result += "</option>";
  }
  result += "</select>";
  return result;
}

String getEventSubmitButtons(String buttonText1, String buttonText2) {
  String result = "<input type=\"submit\" class=\"btn btn-primary btn-sm\" ";
  result += "name=\"";
  result += buttonText1;
  result += "\" value=\"";
  result += buttonText1;
  result += "\">&nbsp;";
  result += "<input type=\"submit\" class=\"btn btn-primary btn-sm\" ";
  result += "name=\"";
  result += buttonText2;
  result += "\" value=\"";
  result += buttonText2;
  result += "\">";
  return result;
}

/**
   Get the header for an input form
*/
String getFormHead(String formTitle) {
  String tableHead = "<div class=\"container\" role=\"main\"><h3 class=\"sub-header\">";
  tableHead += formTitle;
  tableHead += "</h3><form class=\"form-horizontal\">";

  return tableHead;
}

/**
   Get the header for an input form
*/
String getFormFoot() {
  return "</form></div>";
}

String getHTMLFoot() {
  return "</body></html>";
}

String getRadioGroupHeader(String header) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-3\">";
  result += header;
  result += "</label>";
  return result;
}

String getRadioButton(String group_name, String text, String value, boolean checked) {
  String result = "<div class=\"col-xs-1\">";
  if (checked) {
    result += "<label class=\"radio-inline\"><input checked type=\"radio\" name=\"";
  } else {
    result += "<label class=\"radio-inline\"><input type=\"radio\" name=\"";
  }
  result += group_name;
  result += "\" value=\"";
  result += value;
  result += "\"> ";
  result += text;
  result += "</label></div>";
  return result;
}

String getRadioGroupFooter() {
  String result = "</div>";
  return result;
}

String getCheckBox(String checkbox_name, String value, String text, boolean checked) {
  String result = "<div class=\"form-group\"><div class=\"col-xs-offset-3 col-xs-9\"><label class=\"checkbox-inline\">";
  if (checked) {
    result += "<input checked type=\"checkbox\" name=\"";
  } else {
    result += "<input type=\"checkbox\" name=\"";
  }

  result += checkbox_name;
  result += "\" value=\"";
  result += value;
  result += "\"> ";
  result += text;
  result += "</label></div></div>";

  return result;
}

String getDropDownHeader(String heading, String group_name, boolean wide, boolean disabled) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-3\">";
  result += heading;
  if (wide) {
    result += "</label><div class=\"col-xs-8\"><select class=\"form-control\" name=\"";
  } else {
    result += "</label><div class=\"col-xs-2\"><select class=\"form-control\" name=\"";
  }
  result += group_name;
  result += "\"";
  if (disabled) result += " disabled";
  result += ">";
  return result;
}

String getDropDownOption (String value, String text, boolean checked) {
  String result = "";
  if (checked) {
    result += "<option selected value=\"";
  } else {
    result += "<option value=\"";
  }
  result += value;
  result += "\">";
  result += text;
  result += "</option>";
  return result;
}

String getDropDownFooter() {
  return "</select></div></div>";
}

String getNumberInput(String heading, String input_name, unsigned int minVal, unsigned int maxVal, unsigned int value, boolean disabled) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"" + input_name + "\">" + heading;
  result += "</label><div class=\"col-xs-2\"><input type=\"number\" class=\"form-control\" name=\"" + input_name;
  result += "\" id=\"" + input_name;
  result += "\" min=\"" + String(minVal);
  result += "\" max=\"" + String(maxVal);
  result += "\" value=\"" + String(value) + "\"";
  if (disabled) result += " disabled";
  result += "></div></div>";

  return result;
}

String getNumberInputWide(String heading, String input_name, byte minVal, byte maxVal, byte value, boolean disabled) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-8\" for=\"" +input_name + "\">" + heading;
  result += "</label><div class=\"col-xs-2\"><input type=\"number\" class=\"form-control\" name=\"" + input_name;
  result += "\" id=\"" + input_name;
  result += "\" min=\"" + String(minVal);
  result += "\" max=\"" + String(maxVal);
  result += "\" value=\"" + String(value) + "\"";
  if (disabled) result += " disabled";
  result += "></div></div>";

  return result;
}

String getTextInput(String heading, String input_name, String value, boolean disabled) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"" + input_name + "\">" + heading;
  result += "</label><div class=\"col-xs-2\"><input type=\"text\" class=\"form-control\" name=\"" + input_name;
  result += "\" id=\"" + input_name;
  result += "\" value=\"" + value + "\"";
  if (disabled) result += " disabled";
  result += "></div></div>";

  return result;
}

String getTextInputWide(String heading, String input_name, String value, boolean disabled) {
  String result = "<div class=\"form-group\"><label class=\"control-label col-xs-3\" for=\"" + input_name + "\">" + heading;
  result += "</label><div class=\"col-xs-8\"><input type=\"text\" class=\"form-control\" name=\"" + input_name;
  result += "\" id=\"" + input_name;
  result += "\" value=\"" + value + "\"";
  if (disabled) result += " disabled";
  result += "></div></div>";

  return result;
}

String getSubmitButton(String buttonText) {
  String result = "<div class=\"form-group\"><div class=\"col-xs-offset-3 col-xs-9\"><input type=\"submit\" class=\"btn btn-primary\" value=\"";
  result += buttonText;
  result += "\"></div></div>";
  return result;
}
