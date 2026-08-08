/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
//
// Stub implementation of the ProOverlay::TypingOverlay widget.
// The Typing Overlay feature is not yet ported to this fork; this stub
// keeps ProSettings::Storage compiling 1:1 with the reference implementation
// so the overlay can be enabled later by replacing this file alone.
//
#include "settings/pro/pro_typing_overlay.h"

#include <QPaintEvent>
#include <QMouseEvent>

namespace ProOverlay {

TypingOverlay::TypingOverlay() : QWidget(nullptr) {
	setAttribute(Qt::WA_TransparentForMouseEvents);
	hide();
}

void TypingOverlay::showTyping(
		const QString &userName,
		uint64 peerId,
		Main::Session *session,
		const Config &config) {
	// No-op stub: overlay rendering not implemented in this fork yet.
}

void TypingOverlay::paintEvent(QPaintEvent *e) {
	// No-op stub.
}

void TypingOverlay::mousePressEvent(QMouseEvent *e) {
	// No-op stub.
}

void TypingOverlay::hideOverlay() {
	hide();
}

void TypingOverlay::updatePosition(const Config &config) {
	// No-op stub.
}

void TypingOverlay::applySize(Size size) {
	// No-op stub.
}

} // namespace ProOverlay
