/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_exc.h"

#include "settings/settings_common_session.h"
#include "settings/settings_builder.h"
#include "lang/lang_keys.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#ifdef Q_OS_MAC
#include "platform/mac/sparkle_mac.h"
#endif // Q_OS_MAC

namespace Settings {
namespace {

class Exc : public Section<Exc> {
public:
	Exc(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

const auto kMeta = BuildHelper({
	.id = Exc::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_exc,
	.icon = &st::menuIconRestore,
}, [](SectionBuilder &builder) {
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addSubsectionTitle(tr::lng_settings_exc_updates());

	builder.addButton({
		.id = u"exc/check_updates"_q,
		.title = tr::lng_settings_exc_check_updates(),
		.icon = { &st::menuIconRestore },
		.onClick = [=] {
#ifdef Q_OS_MAC
			Platform::CheckForUpdates();
#else // Q_OS_MAC
			if (controller) {
				controller->showToast({
					.text = { tr::lng_settings_exc_check_updates_mac_only(tr::now) },
				});
			}
#endif // Q_OS_MAC
		},
		.keywords = { u"update"_q, u"check"_q, u"sparkle"_q, u"exc"_q },
	});

	builder.addSkip();
	builder.addDivider();
});

const SectionBuildMethod kExcSection = kMeta.build;

Exc::Exc(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> Exc::title() {
	return tr::lng_settings_exc();
}

void Exc::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	build(content, kExcSection);

	Ui::ResizeFitChild(this, content);
}

} // namespace

Type ExcId() {
	return Exc::Id();
}

} // namespace Settings
