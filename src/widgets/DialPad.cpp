#include "widgets/DialPad.h"
#include "utils/SoundPlayer.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

DialPad::DialPad(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DialPad::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 6, 10, 6);

    // Header
    auto* header = new QLabel("SkypeOut - Dial a Number", this);
    header->setAlignment(Qt::AlignCenter);
    QFont hf = header->font();
    hf.setBold(true);
    header->setFont(hf);
    mainLayout->addWidget(header);

    // Number display
    m_numberDisplay = new QLineEdit(this);
    m_numberDisplay->setAlignment(Qt::AlignCenter);
    m_numberDisplay->setReadOnly(false);
    m_numberDisplay->setMinimumHeight(28);
    QFont displayFont = m_numberDisplay->font();
    displayFont.setPointSize(12);
    m_numberDisplay->setFont(displayFont);
    m_numberDisplay->setPlaceholderText("Enter SKP-XXXXX or number");
    mainLayout->addWidget(m_numberDisplay);

    mainLayout->addSpacing(10);

    // Digit grid
    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    struct ButtonInfo { QString digit; QString letters; int row; int col; };
    QList<ButtonInfo> buttons = {
        {"1", "",     0, 0}, {"2", "ABC",  0, 1}, {"3", "DEF",  0, 2},
        {"4", "GHI",  1, 0}, {"5", "JKL",  1, 1}, {"6", "MNO",  1, 2},
        {"7", "PQRS", 2, 0}, {"8", "TUV",  2, 1}, {"9", "WXYZ", 2, 2},
        {"*", "",     3, 0}, {"0", "+",    3, 1}, {"#", "",     3, 2},
    };

    for (auto& info : buttons) {
        auto* btn = createDigitButton(info.digit, info.letters);
        grid->addWidget(btn, info.row, info.col);
    }

    mainLayout->addLayout(grid);
    mainLayout->addSpacing(10);

    // Call and Clear buttons
    auto* buttonLayout = new QHBoxLayout();

    auto* clearBtn = new QPushButton("Clear", this);
    clearBtn->setMinimumHeight(28);
    connect(clearBtn, &QPushButton::clicked, this, &DialPad::onClearClicked);
    buttonLayout->addWidget(clearBtn);

    auto* callBtn = new QPushButton("&Call", this);
    callBtn->setMinimumHeight(23);
    connect(callBtn, &QPushButton::clicked, this, &DialPad::onCallClicked);
    buttonLayout->addWidget(callBtn);

    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
}

QPushButton* DialPad::createDigitButton(const QString& digit, const QString& letters) {
    auto* btn = new QPushButton(this);
    btn->setMinimumSize(55, 40);

    QFont f = btn->font();
    f.setPointSize(9);
    btn->setFont(f);

    if (letters.isEmpty()) {
        btn->setText(digit);
    } else {
        btn->setText(QString("%1\n%2").arg(digit, letters));
    }

    connect(btn, &QPushButton::clicked, [this, digit]() {
        onDigitPressed(digit);
    });

    return btn;
}

void DialPad::onDigitPressed(const QString& digit) {
    m_numberDisplay->setText(m_numberDisplay->text() + digit);
    SoundPlayer::instance().play("VC_BEEP_1.WAV");
}

void DialPad::onCallClicked() {
    QString input = m_numberDisplay->text().trimmed();
    if (input.isEmpty()) return;

    // Check if it's a Skype Number (SKP-XXXXX)
    if (input.startsWith("SKP-", Qt::CaseInsensitive) && input.length() >= 9) {
        emit callSkypeNumber(input.toUpper());
    }
    SoundPlayer::instance().play("CALL_OUT.WAV");
}

void DialPad::onClearClicked() {
    m_numberDisplay->clear();
}
