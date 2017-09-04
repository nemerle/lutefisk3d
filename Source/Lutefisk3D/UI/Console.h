//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once
#include "Lutefisk3D/Core/Object.h"
#include "jlsignal/SignalBase.h"
#include <QtCore/QMap>
#include <utility>

namespace Urho3D
{
enum LogLevels : int32_t;
class Button;
class BorderImage;
class DropDownList;
class Engine;
class Font;
class LineEdit;
class ListView;
class Text;
class UIElement;
class XMLFile;
class IConsoleCommandHandler {
    virtual const QString &name() const=0;
    virtual void handleCommand() = 0;
};
/// %Console window with log history and command line prompt.
class LUTEFISK3D_EXPORT Console : public Object, public jl::SignalObserver
{
    URHO3D_OBJECT(Console,Object)

public:
    /// Construct.
    Console(Context* context);
    /// Destruct.
    ~Console();

    /// Set UI elements' style from an XML file.
    void SetDefaultStyle(XMLFile* style);
    /// Show or hide.
    void SetVisible(bool enable);
    /// Toggle visibility.
    void Toggle();
    /// Automatically set console to visible when receiving an error log message.
    void SetAutoVisibleOnError(bool enable) { autoVisibleOnError_ = enable; }
    /// Set the command interpreter.
    void SetCommandInterpreter(const QString& interpreter) { commandInterpreter_ = interpreter; }
    /// Set number of buffered rows.
    void SetNumBufferedRows(unsigned rows);
    /// Set number of displayed rows.
    void SetNumRows(unsigned rows);
    /// Set command history maximum size, 0 disables history.
    void SetNumHistoryRows(unsigned rows);
    /// Set whether to automatically focus the line edit when showing. Default true on desktops and false on mobile devices, as on mobiles it would pop up the screen keyboard.
    void SetFocusOnShow(bool enable);
    /// Add auto complete option.
    void AddAutoComplete(const QString& option);
    /// Remove auto complete option.
    void RemoveAutoComplete(const QString& option);
    /// Update elements to layout properly. Call this after manually adjusting the sub-elements.
    void UpdateElements();

    /// Return the UI style file.
    XMLFile* GetDefaultStyle() const;
    /// Return the background element.
    BorderImage* GetBackground() const { return background_; }
    /// Return the line edit element.
    LineEdit* GetLineEdit() const { return lineEdit_; }
    /// Return the close butoon element.
    Button* GetCloseButton() const { return closeButton_; }
    /// Return whether is visible.
    bool IsVisible() const;
    /// Return true when console is set to automatically visible when receiving an error log message.
    bool IsAutoVisibleOnError() const { return autoVisibleOnError_; }
    /// Return the last used command interpreter.
    const QString& GetCommandInterpreter() const { return commandInterpreter_; }
    /// Return number of buffered rows.
    unsigned GetNumBufferedRows() const;
    /// Return number of displayed rows.
    unsigned GetNumRows() const { return displayedRows_; }
    /// Copy selected rows to system clipboard.
    void CopySelectedRows() const;
    /// Return history maximum size.
    unsigned GetNumHistoryRows() const { return historyRows_; }
    /// Return current history position.
    unsigned GetHistoryPosition() const { return historyPosition_; }
    /// Return history row at index.
    const QString& GetHistoryRow(unsigned index) const;
    /// Return whether automatically focuses the line edit when showing.
    bool GetFocusOnShow() const { return focusOnShow_; }

private:
    /// Populate the command line interpreters that could handle the console command.
    bool PopulateInterpreter();
    /// Handle interpreter being selected on the drop down list.
    void HandleInterpreterSelected(StringHash eventType, VariantMap& eventData);
    /// Handle text change in the line edit.
    void HandleTextChanged(StringHash eventType, VariantMap& eventData);
    /// Handle enter pressed on the line edit.
    void HandleTextFinished(UIElement *, const QString &, float);
    /// Handle unhandled key on the line edit for scrolling the history.
    void HandleLineEditKey(StringHash eventType, VariantMap& eventData);
    /// Handle close button being pressed.
    void HandleCloseButtonPressed(UIElement *);
    /// Handle UI root resize.
    void HandleRootElementResized(UIElement *, int, int, int, int);
    /// Handle a log message.
    void HandleLogMessage(LogLevels level, const QString &msg);
    /// Handle the application post-update.
    void HandlePostUpdate(float ts);

    /// Auto visible on error flag.
    bool autoVisibleOnError_;
    /// Background.
    SharedPtr<BorderImage> background_;
    /// Container for text rows.
    ListView* rowContainer_;
    /// Container for the command line.
    UIElement* commandLine_;
    /// Interpreter drop down list.
    DropDownList* interpreters_;
    /// Line edit.
    LineEdit* lineEdit_;
    /// Close button.
    SharedPtr<Button> closeButton_;
    /// Last used command interpreter.
    QString commandInterpreter_;
    /// Command history.
    std::vector<QString> history_;
    /// Pending log message rows.
    std::vector<std::pair<int, QString> > pendingRows_;
    /// Current row being edited.
    QString currentRow_;
    /// Maximum displayed rows.
    unsigned displayedRows_;
    /// Command history maximum rows.
    unsigned historyRows_;
    /// Command history current position.
    unsigned historyPosition_;
    /**
    Command auto complete options.

    down arrow key
    Unless currently going through history options, will loop through next auto complete options.

    up arrow key
    Unless currently going through history options, will go through previous auto complete options.
    When no previous options are left will start going through history options.
    */
    QMap<QString,bool> autoComplete_;
    /// Command auto complete current position.
    QMap<QString,bool>::iterator autoCompletePosition_;
    /// Store the original line which is being auto-completed
    QString autoCompleteLine_;
    /// Flag when printing messages to prevent endless loop.
    bool printing_;
    /// Flag for automatically focusing the line edit on showing the console.
    bool focusOnShow_;
    /// Internal flag whether currently in an autocomplete or history change.
    bool historyOrAutoCompleteChange_;
};

}
