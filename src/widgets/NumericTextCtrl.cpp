/**********************************************************************

  Audacity: A Digital Audio Editor

  NumericTextCtrl.cpp

  Dominic Mazzoni


**********************************************************************/



#include "NumericTextCtrl.h"

#include "SampleCount.h"
#include "AllThemeResources.h"
#include "AColor.h"
#include "BasicMenu.h"
#include "../KeyboardCapture.h"
#include "Theme.h"
#include "wxWidgetsWindowPlacement.h"

#include <algorithm>
#include <math.h>
#include <limits>

#include <wx/setup.h> // for wxUSE_* macros

#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <wx/menu.h>
#include <wx/stattext.h>
#include <wx/tooltip.h>
#include <wx/toplevel.h>

#if wxUSE_ACCESSIBILITY
#include "WindowAccessible.h"

class NumericTextCtrlAx final : public WindowAccessible
{
public:
   NumericTextCtrlAx(NumericTextCtrl * ctrl);

   virtual ~ NumericTextCtrlAx();

   // Performs the default action. childId is 0 (the action for this object)
   // or > 0 (the action for a child).
   // Return wxACC_NOT_SUPPORTED if there is no default action for this
   // window (e.g. an edit control).
   wxAccStatus DoDefaultAction(int childId) override;

   // Retrieves the address of an IDispatch interface for the specified child.
   // All objects must support this property.
   wxAccStatus GetChild(int childId, wxAccessible **child) override;

   // Gets the number of children.
   wxAccStatus GetChildCount(int *childCount) override;

   // Gets the default action for this object (0) or > 0 (the action for a child).
   // Return wxACC_OK even if there is no action. actionName is the action, or the empty
   // string if there is no action.
   // The retrieved string describes the action that is performed on an object,
   // not what the object does as a result. For example, a toolbar button that prints
   // a document has a default action of "Press" rather than "Prints the current document."
   wxAccStatus GetDefaultAction(int childId, wxString *actionName) override;

   // Returns the description for this object or a child.
   wxAccStatus GetDescription(int childId, wxString *description) override;

   // Gets the window with the keyboard focus.
   // If childId is 0 and child is NULL, no object in
   // this subhierarchy has the focus.
   // If this object has the focus, child should be 'this'.
   wxAccStatus GetFocus(int *childId, wxAccessible **child) override;

   // Returns help text for this object or a child, similar to tooltip text.
   wxAccStatus GetHelpText(int childId, wxString *helpText) override;

   // Returns the keyboard shortcut for this object or child.
   // Return e.g. ALT+K
   wxAccStatus GetKeyboardShortcut(int childId, wxString *shortcut) override;

   // Returns the rectangle for this object (id = 0) or a child element (id > 0).
   // rect is in screen coordinates.
   wxAccStatus GetLocation(wxRect & rect, int elementId) override;

   // Gets the name of the specified object.
   wxAccStatus GetName(int childId, wxString *name) override;

   // Returns a role constant.
   wxAccStatus GetRole(int childId, wxAccRole *role) override;

   // Gets a variant representing the selected children
   // of this object.
   // Acceptable values:
   // - a null variant (IsNull() returns TRUE)
   // - a list variant (GetType() == wxT("list"))
   // - an integer representing the selected child element,
   //   or 0 if this object is selected (GetType() == wxT("long"))
   // - a "void*" pointer to a wxAccessible child object
   wxAccStatus GetSelections(wxVariant *selections) override;

   // Returns a state constant.
   wxAccStatus GetState(int childId, long *state) override;

   // Returns a localized string representing the value for the object
   // or child.
   wxAccStatus GetValue(int childId, wxString *strValue) override;

private:
   NumericTextCtrl *mCtrl;
   int mLastField;
   int mLastDigit;
   wxString mCachedName;
   wxString mLastCtrlString;
};

#endif // wxUSE_ACCESSIBILITY

#define ID_MENU 9800

// Custom events

DEFINE_EVENT_TYPE(EVT_TIMETEXTCTRL_UPDATED)
DEFINE_EVENT_TYPE(EVT_FREQUENCYTEXTCTRL_UPDATED)
DEFINE_EVENT_TYPE(EVT_BANDWIDTHTEXTCTRL_UPDATED)

BEGIN_EVENT_TABLE(NumericTextCtrl, wxControl)
   EVT_ERASE_BACKGROUND(NumericTextCtrl::OnErase)
   EVT_PAINT(NumericTextCtrl::OnPaint)
   EVT_CONTEXT_MENU(NumericTextCtrl::OnContext)
   EVT_MOUSE_EVENTS(NumericTextCtrl::OnMouse)
   EVT_KEY_DOWN(NumericTextCtrl::OnKeyDown)
   EVT_KEY_UP(NumericTextCtrl::OnKeyUp)
   EVT_SET_FOCUS(NumericTextCtrl::OnFocus)
   EVT_KILL_FOCUS(NumericTextCtrl::OnFocus)
   EVT_COMMAND(wxID_ANY, EVT_CAPTURE_KEY, NumericTextCtrl::OnCaptureKey)
END_EVENT_TABLE()

IMPLEMENT_CLASS(NumericTextCtrl, wxControl)

NumericTextCtrl::NumericTextCtrl(wxWindow *parent, wxWindowID id,
                           NumericConverter::Type type,
                           const NumericFormatSymbol &formatName,
                           double timeValue,
                           double sampleRate,
                           const Options &options,
                           const wxPoint &pos,
                           const wxSize &size):
   wxControl(parent, id, pos, size, wxSUNKEN_BORDER | wxWANTS_CHARS),
   NumericConverter(type, formatName, timeValue, sampleRate),
   mBackgroundBitmap{},
   mDigitFont{},
   mLabelFont{},
   mLastField(1),
   mAutoPos(options.autoPos)
   , mType(type)
{
   mAllowInvalidValue = false;

   mDigitBoxW = 11;
   mDigitBoxH = 19;

   mBorderLeft = 1;
   mBorderTop = 1;
   mBorderRight = 1;
   mBorderBottom = 1;

   mReadOnly = options.readOnly;
   mMenuEnabled = options.menuEnabled;
   mButtonWidth = mMenuEnabled ? 9 : 0;

   SetLayoutDirection(wxLayout_LeftToRight);
   Layout();
   Fit();
   ValueToControls();

   //PRL -- would this fix the following?
   //ValueToControls();

   //mchinen - aug 15 09 - this seems to put the mValue back to zero, and do nothing else.
   //ControlsToValue();

   mScrollRemainder = 0.0;

#if wxUSE_ACCESSIBILITY
   SetLabel(wxT(""));
   SetName( {} );
   SetAccessible(safenew NumericTextCtrlAx(this));
#endif

   if (options.hasInvalidValue)
      SetInvalidValue( options.invalidValue );

   if (!options.format.formatStr.empty())
      SetFormatString( options.format );

   if (options.hasValue)
      SetValue( options.value );
}

NumericTextCtrl::~NumericTextCtrl()
{
}

void NumericTextCtrl::SetName( const TranslatableString &name )
{
   wxControl::SetName( name.Translation() );
}

// Set the focus to the first (left-most) non-zero digit
// If all digits are zero, the right-most position is focused
// If all digits are hyphens (invalid), the left-most position is focused
void NumericTextCtrl::UpdateAutoFocus()
{
   if (!mAutoPos)
      return;

   mFocusedDigit = 0;
   while (mFocusedDigit < ((int)mDigits.size() - 1)) {
      wxChar dgt = mValueString[mDigits[mFocusedDigit].pos];
      if (dgt != '0') {
         break;
      }
      mFocusedDigit++;
   }
}

bool NumericTextCtrl::SetFormatName(const NumericFormatSymbol & formatName)
{
   return
      SetFormatString(GetBuiltinFormat(formatName));
}

bool NumericTextCtrl::SetFormatString(const FormatStrings & formatString)
{
   auto result =
      NumericConverter::SetFormatString(formatString);
   if (result) {
      mBoxes.clear();
      Layout();
      Fit();
      ValueToControls();
      ControlsToValue();
      UpdateAutoFocus();
   }
   return result;
}

void NumericTextCtrl::SetSampleRate(double sampleRate)
{
   NumericConverter::SetSampleRate(sampleRate);
   mBoxes.clear();
   Layout();
   Fit();
   ValueToControls();
   ControlsToValue();
}

void NumericTextCtrl::SetValue(double newValue)
{
   NumericConverter::SetValue(newValue);
   ValueToControls();
   ControlsToValue();
}

void NumericTextCtrl::SetDigitSize(int width, int height)
{
   mDigitBoxW = width;
   mDigitBoxH = height;
   Layout();
   Fit();
}

void NumericTextCtrl::SetReadOnly(bool readOnly)
{
   mReadOnly = readOnly;
}

void NumericTextCtrl::EnableMenu(bool enable)
{
#if wxUSE_TOOLTIPS
   wxString tip(_("(Use context menu to change format.)"));
   if (enable)
      SetToolTip(tip);
   else {
      wxToolTip *tt = GetToolTip();
      if (tt && tt->GetTip() == tip)
         SetToolTip(NULL);
   }
#endif
   mMenuEnabled = enable;
   mButtonWidth = enable ? 9 : 0;
   Layout();
   Fit();
}

void NumericTextCtrl::SetInvalidValue(double invalidValue)
{
   const bool wasInvalid = mAllowInvalidValue && (mValue == mInvalidValue);
   mAllowInvalidValue = true;
   mInvalidValue = invalidValue;
   if (wasInvalid)
      SetValue(invalidValue);
}

wxSize NumericTextCtrl::ComputeSizing(bool update, wxCoord boxW, wxCoord boxH)
{
   // Get current box size
   if (boxW == 0) {
      boxW = mDigitBoxW;
   }

   if (boxH == 0) {
      boxH = mDigitBoxH;
   }
   boxH -= (mBorderTop + mBorderBottom);

   // We can use the screen device context since we're not drawing to it
   wxScreenDC dc;

   // First calculate a rough point size
   wxFont pf(wxSize(boxW, boxH), wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
   int fontSize = pf.GetPointSize();
   wxCoord strW;
   wxCoord strH;

   // Now decrease it until we fit within our digit box
   dc.SetFont(pf);
   dc.GetTextExtent(wxT("0"), &strW, &strH);
   while (strW > boxW || strH > boxH) {
      dc.SetFont(wxFont(--fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
      dc.GetTextExtent(wxT("0"), &strW, &strH);
   }
   fontSize--;

   // Create the digit font with the new point size
   if (update) {
      mDigitFont = std::make_unique<wxFont>(fontSize, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
      dc.SetFont(*mDigitFont);

      // Remember the actual digit width and height using the new font
      dc.GetTextExtent(wxT("0"), &mDigitW, &mDigitH);
   }

   // The label font should be a little smaller
   std::unique_ptr<wxFont> labelFont = std::make_unique<wxFont>(fontSize - 1, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

   // Use the label font for all remaining measurements since only non-digit text is left
   dc.SetFont(*labelFont);
 
   // Remember the pointer if updating
   if (update) {
      mLabelFont = std::move(labelFont);
   }

   // Get the width of the prefix, if any
   dc.GetTextExtent(mPrefix, &strW, &strH);

   // Bump x-position to the end of the prefix
   int x = mBorderLeft + strW;

   if (update) {
      // Set the character position past the prefix
      int pos = mPrefix.length();

      // Reset digits array
      mDigits.clear();
      mBoxes.clear();

      // Figure out the x-position of each field and label in the box
      for (int i = 0, fcnt = mFields.size(); i < fcnt; ++i) {
         // Get the size of the label
         dc.GetTextExtent(mFields[i].label, &strW, &strH);

         // Remember this field's x-position
         mFields[i].fieldX = x;

         // Remember metrics for each digit
         for (int j = 0, dcnt = mFields[i].digits; j < dcnt; ++j) {
            mDigits.push_back(DigitInfo(i, j, pos));
            mBoxes.push_back(wxRect{ x, mBorderTop, boxW, boxH });
            x += boxW;
            pos++;
         }

         // Remember the label's x-position
         mFields[i].labelX = x;

         // Bump to end of label
         x += strW;

         // Remember the label's width
         mFields[i].fieldW = x;

         // Bump character position to end of label
         pos += mFields[i].label.length();
      }
   }
   else {
      // Determine the maximum x-position (length) of the remaining fields
      for (int i = 0, fcnt = mFields.size(); i < fcnt; ++i) {
         // Get the size of the label
         dc.GetTextExtent(mFields[i].label, &strW, &strH);

         // Just bump to next field
         x += (boxW * mFields[i].digits) + strW;
      }
   }

   // Calculate the maximum dimensions
   wxSize dim(x + mBorderRight, boxH + mBorderTop + mBorderBottom);

   // Save maximumFinally, calculate the minimum dimensions
   if (update) {
      mWidth = dim.x;
      mHeight = dim.y;
   }

   return wxSize(dim.x + mButtonWidth, dim.y);
}

bool NumericTextCtrl::Layout()
{
   ComputeSizing();

   wxMemoryDC memDC;
   wxCoord strW, strH;
   memDC.SetFont(*mLabelFont);
   memDC.GetTextExtent(mPrefix, &strW, &strH);

   int i;

   // Draw the background bitmap - it contains black boxes where
   // all of the digits go and all of the other text

   wxBrush Brush;

   mBackgroundBitmap = std::make_unique<wxBitmap>(mWidth + mButtonWidth, mHeight,24);
   memDC.SelectObject(*mBackgroundBitmap);

   theTheme.SetBrushColour( Brush, clrTimeHours );
   memDC.SetBrush(Brush);
   memDC.SetPen(*wxTRANSPARENT_PEN);
   memDC.DrawRectangle(0, 0, mWidth + mButtonWidth, mHeight);

   int numberBottom = mBorderTop + (mDigitBoxH - mDigitH)/2 + mDigitH;

   memDC.GetTextExtent(wxT("0"), &strW, &strH);
   int labelTop = numberBottom - strH;

   memDC.SetTextForeground(theTheme.Colour( clrTimeFont ));
   memDC.SetTextBackground(theTheme.Colour( clrTimeBack ));
   memDC.DrawText(mPrefix, mBorderLeft, labelTop);

   theTheme.SetBrushColour( Brush, clrTimeBack );
   memDC.SetBrush(Brush);
   //memDC.SetBrush(*wxLIGHT_GREY_BRUSH);
   for(i = 0; i < mDigits.size(); i++)
      memDC.DrawRectangle(GetBox(i));
   memDC.SetBrush( wxNullBrush );

   for(i = 0; i < mFields.size(); i++)
      memDC.DrawText(mFields[i].label,
                     mFields[i].labelX, labelTop);

   if (mMenuEnabled) {
      wxRect r(mWidth, 0, mButtonWidth - 1, mHeight - 1);
      AColor::Bevel(memDC, true, r);
      memDC.SetBrush(*wxBLACK_BRUSH);
      memDC.SetPen(*wxBLACK_PEN);
      AColor::Arrow(memDC,
                    mWidth + 1,
                    (mHeight / 2) - 2,
                    mButtonWidth - 2);
   }
   return true;
}

void NumericTextCtrl::Fit()
{
   wxSize sz = GetSize();
   wxSize csz = GetClientSize();

   sz.x = mButtonWidth + mWidth + (sz.x - csz.x);
   sz.y = mHeight + (sz.y - csz.y);

   SetInitialSize(sz);
}

void NumericTextCtrl::OnErase(wxEraseEvent & WXUNUSED(event))
{
   // Ignore it to prevent flashing
}

void NumericTextCtrl::OnPaint(wxPaintEvent & WXUNUSED(event))
{
   wxBufferedPaintDC dc(this);
   bool focused = (FindFocus() == this);

   dc.DrawBitmap(*mBackgroundBitmap, 0, 0);

   wxPen   Pen;
   wxBrush Brush;
   if (focused) {
      theTheme.SetPenColour( Pen, clrTimeFontFocus );
      dc.SetPen(Pen);
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.DrawRectangle(0, 0, mWidth, mHeight);
      dc.SetPen( wxNullPen );
   }

   dc.SetFont(*mDigitFont);
   dc.SetTextForeground(theTheme.Colour( clrTimeFont ));
   dc.SetTextBackground(theTheme.Colour( clrTimeBack ));

   dc.SetPen(*wxTRANSPARENT_PEN);
   theTheme.SetBrushColour( Brush , clrTimeBackFocus );
   dc.SetBrush( Brush );

   int i;
   for(i = 0; i < (int)mDigits.size(); i++) {
      wxRect box = GetBox(i);
      if (focused && mFocusedDigit == i) {
         dc.DrawRectangle(box);
         dc.SetTextForeground(theTheme.Colour( clrTimeFontFocus ));
         dc.SetTextBackground(theTheme.Colour( clrTimeBackFocus ));
      }
      int pos = mDigits[i].pos;
      wxString digit = mValueString.Mid(pos, 1);
      int x = box.x + (mDigitBoxW - mDigitW)/2;
      int y = box.y + (mDigitBoxH - mDigitH)/2;
      dc.DrawText(digit, x, y);
      if (focused && mFocusedDigit == i) {
         dc.SetTextForeground(theTheme.Colour( clrTimeFont ));
         dc.SetTextBackground(theTheme.Colour( clrTimeBack ));
      }
   }
   dc.SetPen( wxNullPen );
   dc.SetBrush( wxNullBrush );
}

void NumericTextCtrl::OnContext(wxContextMenuEvent &event)
{
   wxMenu menu;
   int i;

   if (!mMenuEnabled) {
      event.Skip();
      return;
   }

   SetFocus();

   int currentSelection = -1;
   for (i = 0; i < GetNumBuiltins(); i++) {
      menu.AppendRadioItem(ID_MENU + i, GetBuiltinName(i).Translation());
      if (mFormatString == GetBuiltinFormat(i)) {
         menu.Check(ID_MENU + i, true);
         currentSelection = i;
      }
   }

   menu.Bind(wxEVT_MENU, [](auto&){});
   BasicMenu::Handle{ &menu }.Popup(
      wxWidgetsWindowPlacement{ this },
      { 0, 0 }
   );

   // This used to be in an EVT_MENU() event handler, but GTK
   // is sensitive to what is done within the handler if the
   // user happens to check the first menuitem and then is 
   // moving down the menu when the ...CTRL_UPDATED event
   // handler kicks in.
   for (i = 0; i < GetNumBuiltins(); i++) {
      if (menu.IsChecked(ID_MENU + i) && i != currentSelection) {
         SetFormatString(GetBuiltinFormat(i));
      
         int eventType = 0;
         switch (mType) {
            case NumericConverter::TIME:
               eventType = EVT_TIMETEXTCTRL_UPDATED;
               break;
            case NumericConverter::FREQUENCY:
               eventType = EVT_FREQUENCYTEXTCTRL_UPDATED;
               break;
            case NumericConverter::BANDWIDTH:
               eventType = EVT_BANDWIDTHTEXTCTRL_UPDATED;
               break;
            default:
               wxASSERT(false);
               break;
         }
      
         wxCommandEvent e(eventType, GetId());
         e.SetInt(i);
         e.SetString(GetBuiltinName(i).Internal());
         GetParent()->GetEventHandler()->AddPendingEvent(e);
      }
   }

}

void NumericTextCtrl::OnMouse(wxMouseEvent &event)
{
   if (event.LeftDown() && event.GetX() >= mWidth) {
      wxContextMenuEvent e;
      OnContext(e);
   }
   else if (event.LeftDown()) {
      SetFocus();

      int bestDist = 9999;
      unsigned int i;

      mFocusedDigit = 0;
      for(i = 0; i < mDigits.size(); i++) {
         int dist = abs(event.m_x - (GetBox(i).x +
                                     GetBox(i).width/2));
         if (dist < bestDist) {
            mFocusedDigit = i;
            bestDist = dist;
         }
      }

      Refresh(false);
   }
   else if (event.RightDown() && mMenuEnabled) {
      wxContextMenuEvent e;
      OnContext(e);
   }
   else if(!mReadOnly && event.m_wheelRotation != 0 ) {
      double steps = event.m_wheelRotation /
         (event.m_wheelDelta > 0 ? (double)event.m_wheelDelta : 120.0) +
         mScrollRemainder;
      mScrollRemainder = steps - floor(steps);
      steps = floor(steps);

      Adjust((int)fabs(steps), steps < 0.0 ? -1 : 1);
      Updated();
   }
}

void NumericTextCtrl::OnFocus(wxFocusEvent &event)
{
   KeyboardCapture::OnFocus( *this, event );

   if (event.GetEventType() != wxEVT_KILL_FOCUS &&
       mFocusedDigit <= 0 )
      UpdateAutoFocus();

   event.Skip( false ); // PRL: not sure why, but preserving old behavior
}

void NumericTextCtrl::OnCaptureKey(wxCommandEvent &event)
{
   wxKeyEvent *kevent = (wxKeyEvent *)event.GetEventObject();
   int keyCode = kevent->GetKeyCode();

   // Convert numeric keypad entries.
   if ((keyCode >= WXK_NUMPAD0) && (keyCode <= WXK_NUMPAD9))
      keyCode -= WXK_NUMPAD0 - '0';

   switch (keyCode)
   {
      case WXK_BACK:
      case WXK_LEFT:
      case WXK_RIGHT:
      case WXK_HOME:
      case WXK_END:
      case WXK_UP:
      case WXK_DOWN:
      case WXK_TAB:
      case WXK_RETURN:
      case WXK_NUMPAD_ENTER:
      case WXK_DELETE:
         return;

      default:
         if (keyCode >= '0' && keyCode <= '9' && !kevent->HasAnyModifiers())
            return;
   }

   event.Skip();

   return;
}

void NumericTextCtrl::OnKeyUp(wxKeyEvent &event)
{
   int keyCode = event.GetKeyCode();

   event.Skip(true);

   if ((keyCode >= WXK_NUMPAD0) && (keyCode <= WXK_NUMPAD9))
      keyCode -= WXK_NUMPAD0 - '0';

   if ((keyCode >= '0' && keyCode <= '9' && !event.HasAnyModifiers()) ||
       (keyCode == WXK_DELETE) ||
       (keyCode == WXK_BACK) ||
       (keyCode == WXK_UP) ||
       (keyCode == WXK_DOWN)) {
      Updated(true);
   }
}

void NumericTextCtrl::OnKeyDown(wxKeyEvent &event)
{
   if (mDigits.size() == 0)
   {
      mFocusedDigit = 0;
      return;
   }

   event.Skip(false);

   int keyCode = event.GetKeyCode();
   int digit = mFocusedDigit;

   if (mFocusedDigit < 0)
      mFocusedDigit = 0;
   if (mFocusedDigit >= (int)mDigits.size())
      mFocusedDigit = mDigits.size() - 1;

   // Convert numeric keypad entries.
   if ((keyCode >= WXK_NUMPAD0) && (keyCode <= WXK_NUMPAD9))
      keyCode -= WXK_NUMPAD0 - '0';

   if (!mReadOnly && (keyCode >= '0' && keyCode <= '9' && !event.HasAnyModifiers())) {
      int digitPosition = mDigits[mFocusedDigit].pos;
      if (mValueString[digitPosition] == wxChar('-')) {
         mValue = std::max(mMinValue, std::min(mMaxValue, 0.0));
         ValueToControls();
         // Beware relocation of the string
         digitPosition = mDigits[mFocusedDigit].pos;
      }
      mValueString[digitPosition] = wxChar(keyCode);
      ControlsToValue();
      Refresh();// Force an update of the control. [Bug 1497]
      ValueToControls();
      mFocusedDigit = (mFocusedDigit + 1) % (mDigits.size());
      Updated();
   }

   else if (!mReadOnly && keyCode == WXK_DELETE) {
      if (mAllowInvalidValue)
         SetValue(mInvalidValue);
   }

   else if (!mReadOnly && keyCode == WXK_BACK) {
      // Moves left, replaces that char with '0', stays there...
      mFocusedDigit--;
      mFocusedDigit += mDigits.size();
      mFocusedDigit %= mDigits.size();
      wxString::reference theDigit = mValueString[mDigits[mFocusedDigit].pos];
      if (theDigit != wxChar('-'))
         theDigit = '0';
      ControlsToValue();
      Refresh();// Force an update of the control. [Bug 1497]
      ValueToControls();
      Updated();
   }

   else if (keyCode == WXK_LEFT) {
      mFocusedDigit--;
      mFocusedDigit += mDigits.size();
      mFocusedDigit %= mDigits.size();
      Refresh();
   }

   else if (keyCode == WXK_RIGHT) {
      mFocusedDigit++;
      mFocusedDigit %= mDigits.size();
      Refresh();
   }

   else if (keyCode == WXK_HOME) {
      mFocusedDigit = 0;
      Refresh();
   }

   else if (keyCode == WXK_END) {
      mFocusedDigit = mDigits.size() - 1;
      Refresh();
   }

   else if (!mReadOnly && keyCode == WXK_UP) {
      Adjust(1, 1);
      Updated();
   }

   else if (!mReadOnly && keyCode == WXK_DOWN) {
      Adjust(1, -1);
      Updated();
   }

   else if (keyCode == WXK_TAB) {
#if defined(__WXMSW__)
      // Using Navigate() on Windows, rather than the following code causes
      // bug 1542
      wxWindow* parent = GetParent();
      wxNavigationKeyEvent nevent;
      nevent.SetWindowChange(event.ControlDown());
      nevent.SetDirection(!event.ShiftDown());
      nevent.SetEventObject(parent);
      nevent.SetCurrentFocus(parent);
      GetParent()->GetEventHandler()->ProcessEvent(nevent);
#else
      Navigate(event.ShiftDown()
               ? wxNavigationKeyEvent::IsBackward
               : wxNavigationKeyEvent::IsForward);
#endif
   }

   else if (keyCode == WXK_RETURN || keyCode == WXK_NUMPAD_ENTER) {
      wxTopLevelWindow *tlw = wxDynamicCast(wxGetTopLevelParent(this), wxTopLevelWindow);
      wxWindow *def = tlw->GetDefaultItem();
      if (def && def->IsEnabled()) {
         wxCommandEvent cevent(wxEVT_COMMAND_BUTTON_CLICKED,
                               def->GetId());
         cevent.SetEventObject( def );
         GetParent()->GetEventHandler()->ProcessEvent(cevent);
      }
   }

   else {
      event.Skip();
      return;
   }

   if (digit != mFocusedDigit) {
      SetFieldFocus(mFocusedDigit);
   }
}

void NumericTextCtrl::SetFieldFocus(int  digit)
{
#if wxUSE_ACCESSIBILITY
   if (mDigits.size() == 0)
   {
      mFocusedDigit = 0;
      return;
   }
   mFocusedDigit = digit;
   mLastField = mDigits[mFocusedDigit].field + 1;

   GetAccessible()->NotifyEvent(wxACC_EVENT_OBJECT_FOCUS,
                                this,
                                wxOBJID_CLIENT,
                                mFocusedDigit + 1);
#endif
}

void NumericTextCtrl::Updated(bool keyup /* = false */)
{
   wxCommandEvent event(wxEVT_COMMAND_TEXT_UPDATED, GetId());

   // This will give listeners the ability to do tasks when the
   // update has been completed, like when the UP ARROW has been
   // held down and is finally released.
   event.SetInt(keyup);
   event.SetEventObject(this);
   GetEventHandler()->ProcessEvent(event);

#if wxUSE_ACCESSIBILITY
   if (!keyup) {
      if (mDigits.size() == 0)
      {
         mFocusedDigit = 0;
         return;
      }

      // The object_focus event is only needed by Window-Eyes
      // and can be removed when we cease to support this screen reader.
      GetAccessible()->NotifyEvent(wxACC_EVENT_OBJECT_FOCUS,
         this,
         wxOBJID_CLIENT,
         mFocusedDigit + 1);

      GetAccessible()->NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE,
         this,
         wxOBJID_CLIENT,
         mFocusedDigit + 1);
   }
#endif
}

void NumericTextCtrl::ValueToControls()
{
   const wxString previousValueString = mValueString;
   NumericConverter::ValueToControls(mValue);
   if (mValueString != previousValueString) {
      // Doing this only when needed is an optimization.
      // NumerixTextCtrls are used in the selection bar at the bottom
      // of Audacity, and are updated at high frequency through
      // SetValue() when Audacity is playing. This consumes a
      // significant amount of CPU. Typically, when a track is
      // playing, only one of the NumericTextCtrl actually changes
      // (the audio position). We save CPU by updating the control
      // only when needed.
      Refresh(false);
   }
}


void NumericTextCtrl::ControlsToValue()
{
   NumericConverter::ControlsToValue();
}

wxRect NumericTextCtrl::GetBox(size_t ii) const
{
   if (ii < mBoxes.size())
      return mBoxes[ii];
   return {};
}

#if wxUSE_ACCESSIBILITY

NumericTextCtrlAx::NumericTextCtrlAx(NumericTextCtrl *ctrl)
:  WindowAccessible(ctrl)
{
   mCtrl = ctrl;
   mLastField = -1;
   mLastDigit = -1;
}

NumericTextCtrlAx::~NumericTextCtrlAx()
{
}

// Performs the default action. childId is 0 (the action for this object)
// or > 0 (the action for a child).
// Return wxACC_NOT_SUPPORTED if there is no default action for this
// window (e.g. an edit control).
wxAccStatus NumericTextCtrlAx::DoDefaultAction(int WXUNUSED(childId))
{
   return wxACC_NOT_SUPPORTED;
}

// Retrieves the address of an IDispatch interface for the specified child.
// All objects must support this property.
wxAccStatus NumericTextCtrlAx::GetChild(int childId, wxAccessible **child)
{
   if (childId == wxACC_SELF) {
      *child = this;
   }
   else {
      *child = NULL;
   }

   return wxACC_OK;
}

// Gets the number of children.
wxAccStatus NumericTextCtrlAx::GetChildCount(int *childCount)
{
   *childCount = mCtrl->mDigits.size();

   return wxACC_OK;
}

// Gets the default action for this object (0) or > 0 (the action for
// a child).  Return wxACC_OK even if there is no action. actionName
// is the action, or the empty string if there is no action.  The
// retrieved string describes the action that is performed on an
// object, not what the object does as a result. For example, a
// toolbar button that prints a document has a default action of
// "Press" rather than "Prints the current document."
wxAccStatus NumericTextCtrlAx::GetDefaultAction(int WXUNUSED(childId), wxString *actionName)
{
   actionName->clear();

   return wxACC_OK;
}

// Returns the description for this object or a child.
wxAccStatus NumericTextCtrlAx::GetDescription(int WXUNUSED(childId), wxString *description)
{
   description->clear();

   return wxACC_OK;
}

// Gets the window with the keyboard focus.
// If childId is 0 and child is NULL, no object in
// this subhierarchy has the focus.
// If this object has the focus, child should be 'this'.
wxAccStatus NumericTextCtrlAx::GetFocus(int *childId, wxAccessible **child)
{
   *childId = mCtrl->GetFocusedDigit();
   *child = this;

   return wxACC_OK;
}

// Returns help text for this object or a child, similar to tooltip text.
wxAccStatus NumericTextCtrlAx::GetHelpText(int WXUNUSED(childId), wxString *helpText)
{
// removed help text, as on balance it's more of an irritation than useful
#if 0    // was #if wxUSE_TOOLTIPS
   wxToolTip *pTip = mCtrl->GetToolTip();
   if (pTip) {
      *helpText = pTip->GetTip();
   }

   return wxACC_OK;
#else
   helpText->clear();

   return wxACC_NOT_SUPPORTED;
#endif
}

// Returns the keyboard shortcut for this object or child.
// Return e.g. ALT+K
wxAccStatus NumericTextCtrlAx::GetKeyboardShortcut(int WXUNUSED(childId), wxString *shortcut)
{
   shortcut->clear();

   return wxACC_OK;
}

// Returns the rectangle for this object (id = 0) or a child element (id > 0).
// rect is in screen coordinates.
wxAccStatus NumericTextCtrlAx::GetLocation(wxRect & rect, int elementId)
{
   if ((elementId != wxACC_SELF) &&
         // We subtract 1, below, and need to avoid neg index to mDigits.
         (elementId > 0))
   {
//      rect.x += mCtrl->mFields[elementId - 1].fieldX;
//      rect.width =  mCtrl->mFields[elementId - 1].fieldW;
        rect = mCtrl->GetBox(elementId - 1);
        rect.SetPosition(mCtrl->ClientToScreen(rect.GetPosition()));
   }
   else
   {
      rect = mCtrl->GetRect();
      rect.SetPosition(mCtrl->GetParent()->ClientToScreen(rect.GetPosition()));
   }

   return wxACC_OK;
}

static void GetFraction( wxString &label,
   const NumericConverter::FormatStrings &formatStrings,
   bool isTime, int digits )
{
   TranslatableString tr = formatStrings.fraction;
   if ( tr.empty() ) {
      wxASSERT( isTime );
      if (digits == 2)
         tr = XO("centiseconds");
      else if (digits == 3)
         tr = XO("milliseconds");
   }
   if (!tr.empty())
      label = tr.Translation();
}

// Gets the name of the specified object.
wxAccStatus NumericTextCtrlAx::GetName(int childId, wxString *name)
{
   // Slightly messy trick to save us some prefixing.
   std::vector<NumericField> & mFields = mCtrl->mFields;

   wxString ctrlString = mCtrl->GetString();
   int field = mCtrl->GetFocusedField();

   // Return the entire string including the control label
   // when the requested child ID is wxACC_SELF.  (Mainly when
   // the control gets the focus.)
   if ((childId == wxACC_SELF) ||
         // We subtract 1 from childId in the other cases below, and
         // need to avoid neg index to mDigits, so funnel into this clause.
         (childId < 1))
   {
      *name = mCtrl->GetName();
      if (name->empty()) {
         *name = mCtrl->GetLabel();
      }

      *name += wxT(" ") +
               mCtrl->GetString();
   }
   // This case is needed because of the behaviour of Narrator, which
   // is different for the other Windows screen readers. After a focus event,
   // Narrator causes getName() to be called more than once. However, the code in
   // the following else statement assumes that it is executed only once
   // when the focus has been moved to another digit. This else if statement
   // ensures that this is the case, by using a cached value if nothing
   // has changed.
   else if (childId == mLastDigit && ctrlString.IsSameAs(mLastCtrlString)) {
      *name = mCachedName;
   }
   else {
      // The user has moved from one field of the time to another so
      // report the value of the field and the field's label.
      if (mLastField != field) {
         wxString label = mFields[field - 1].label;
         int cnt = mFields.size();
         wxString decimal = wxLocale::GetInfo(wxLOCALE_DECIMAL_POINT, wxLOCALE_CAT_NUMBER);

         // If the NEW field is the last field, then check it to see if
         // it represents fractions of a second.
         // PRL: click a digit of the control and use left and right arrow keys
         // to exercise this code
         const bool isTime = (mCtrl->mType == NumericTextCtrl::TIME);
         if (field > 1 && field == cnt) {
            if (mFields[field - 2].label == decimal) {
               int digits = mFields[field - 1].digits;
               GetFraction( label, mCtrl->mFormatString,
                  isTime, digits );
            }
         }
         // If the field following this one represents fractions of a
         // second then use that label instead of the decimal point.
         else if (label == decimal && field == cnt - 1) {
            label = mFields[field].label;
         }

         *name = mFields[field - 1].str +
                 wxT(" ") +
                 label +
                 wxT(", ") +     // comma inserts a slight pause
                 mCtrl->GetString().at(mCtrl->mDigits[childId - 1].pos);
         mLastField = field;
         mLastDigit = childId;
      }
      // The user has moved from one digit to another within a field so
      // just report the digit under the cursor.
      else if (mLastDigit != childId) {
         *name = mCtrl->GetString().at(mCtrl->mDigits[childId - 1].pos);
         mLastDigit = childId;
      }
      // The user has updated the value of a field, so report the field's
      // value only.
      else if (field > 0)
      {
         *name = mFields[field - 1].str;
      }

      mCachedName = *name;
      mLastCtrlString = ctrlString;
   }

   return wxACC_OK;
}

// Returns a role constant.
wxAccStatus NumericTextCtrlAx::GetRole(int WXUNUSED(childId), wxAccRole *role)
{
   *role = wxROLE_SYSTEM_STATICTEXT;
   return wxACC_OK;
}

// Gets a variant representing the selected children
// of this object.
// Acceptable values:
// - a null variant (IsNull() returns TRUE)
// - a list variant (GetType() == wxT("list"))
// - an integer representing the selected child element,
//   or 0 if this object is selected (GetType() == wxT("long"))
// - a "void*" pointer to a wxAccessible child object
wxAccStatus NumericTextCtrlAx::GetSelections(wxVariant * WXUNUSED(selections))
{
   return wxACC_NOT_IMPLEMENTED;
}

// Returns a state constant.
wxAccStatus NumericTextCtrlAx::GetState(int WXUNUSED(childId), long *state)
{
   *state = wxACC_STATE_SYSTEM_FOCUSABLE;
   *state |= (mCtrl == wxWindow::FindFocus() ? wxACC_STATE_SYSTEM_FOCUSED : 0);

   return wxACC_OK;
}

// Returns a localized string representing the value for the object
// or child.
wxAccStatus NumericTextCtrlAx::GetValue(int WXUNUSED(childId), wxString * WXUNUSED(strValue))
{
   return wxACC_NOT_IMPLEMENTED;
}

#endif

