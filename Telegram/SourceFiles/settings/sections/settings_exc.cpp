/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "settings/sections/settings_exc.h"

#include "settings/settings_common_session.h"
#include "settings/settings_builder.h"
#include "settings/sections/settings_main.h"
#include "settings/pro/pro_settings_storage.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#ifdef Q_OS_MAC
#include "platform/mac/sparkle_mac.h"
#endif // Q_OS_MAC

namespace Settings {
namespace {

using namespace Builder;

struct ProState {
	ProSettings::Storage *storage = nullptr;
	base::flat_set<not_null<PeerData*>> exceptions;
	rpl::variable<int> exceptionsCount = 0;
	base::flat_set<not_null<PeerData*>> ghostExceptions;
	rpl::variable<int> ghostExceptionsCount = 0;
	base::flat_set<not_null<PeerData*>> editExceptions;
	rpl::variable<int> editExceptionsCount = 0;
};

void InitProState(ProState *state, not_null<Main::Session*> session) {
	state->storage = &session->proStorage();

	for (const auto &id : state->storage->exceptionPeerIds()) {
		const auto peerId = PeerId(id);
		if (peerId) {
			const auto peer = session->data().peer(peerId);
			state->exceptions.emplace(peer);
		}
	}
	state->exceptionsCount = int(state->exceptions.size());

	for (const auto &id : state->storage->ghostExceptionPeerIds()) {
		const auto peerId = PeerId(id);
		if (peerId) {
			const auto peer = session->data().peer(peerId);
			state->ghostExceptions.emplace(peer);
		}
	}
	state->ghostExceptionsCount = int(state->ghostExceptions.size());

	for (const auto &id : state->storage->editExceptionPeerIds()) {
		const auto peerId = PeerId(id);
		if (peerId) {
			const auto peer = session->data().peer(peerId);
			state->editExceptions.emplace(peer);
		}
	}
	state->editExceptionsCount = int(state->editExceptions.size());
}

class GenericExceptionsController final : public PeerListController {
public:
	GenericExceptionsController(
		not_null<Main::Session*> session,
		base::flat_set<not_null<PeerData*>> *peers,
		rpl::variable<int> *count,
		Fn<void(uint64)> onAdd,
		Fn<void(uint64)> onRemove);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;
	void rowRightActionClicked(not_null<PeerListRow*> row) override;
	void loadMoreRows() override {}

	void addPeer(not_null<PeerData*> peer);

private:
	[[nodiscard]] std::unique_ptr<PeerListRow> createRow(
		not_null<PeerData*> peer) const;

	const not_null<Main::Session*> _session;
	base::flat_set<not_null<PeerData*>> * const _peers;
	rpl::variable<int> * const _count;
	Fn<void(uint64)> _onAdd;
	Fn<void(uint64)> _onRemove;
};

GenericExceptionsController::GenericExceptionsController(
	not_null<Main::Session*> session,
	base::flat_set<not_null<PeerData*>> *peers,
	rpl::variable<int> *count,
	Fn<void(uint64)> onAdd,
	Fn<void(uint64)> onRemove)
: _session(session)
, _peers(peers)
, _count(count)
, _onAdd(std::move(onAdd))
, _onRemove(std::move(onRemove)) {
}

Main::Session &GenericExceptionsController::session() const {
	return *_session;
}

void GenericExceptionsController::prepare() {
	for (const auto &peer : *_peers) {
		delegate()->peerListAppendRow(createRow(peer));
	}
	delegate()->peerListRefreshRows();
}

void GenericExceptionsController::rowClicked(not_null<PeerListRow*> row) {
}

void GenericExceptionsController::rowRightActionClicked(
		not_null<PeerListRow*> row) {
	const auto peer = row->peer();
	_peers->remove(peer);
	*_count = int(_peers->size());
	_onRemove(peer->id.value);
	delegate()->peerListRemoveRow(row);
	delegate()->peerListRefreshRows();
}

void GenericExceptionsController::addPeer(not_null<PeerData*> peer) {
	if (!_peers->emplace(peer).second) {
		return;
	}
	*_count = int(_peers->size());
	_onAdd(peer->id.value);
	delegate()->peerListAppendRow(createRow(peer));
	delegate()->peerListRefreshRows();
}

std::unique_ptr<PeerListRow> GenericExceptionsController::createRow(
		not_null<PeerData*> peer) const {
	auto row = std::make_unique<PeerListRowWithLink>(peer);
	row->setActionLink(u"Remove"_q);
	return row;
}

void BuildSaveDeletedSection(SectionBuilder &builder, ProState *state) {
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Deleted Messages"_q));

	const auto toggle = builder.addButton({
		.id = u"exc/save_deleted"_q,
		.title = rpl::single(u"Save deleted messages"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			state ? state->storage->saveDeletedEnabled() : false),
		.keywords = { u"deleted"_q, u"messages"_q, u"save"_q },
	});

	if (toggle && state) {
		toggle->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			state->storage->setSaveDeletedEnabled(enabled);
		}, toggle->lifetime());
	}

	builder.scope([&] {
		struct ExceptionsState {
			std::unique_ptr<GenericExceptionsController> controller;
			std::unique_ptr<PeerListContentDelegateSimple> delegate;
		};

		const auto inner = builder.container();
		GenericExceptionsController *exceptionsController = nullptr;

		if (inner && controller && state) {
			auto listController = std::make_unique<GenericExceptionsController>(
				&controller->session(),
				&state->exceptions,
				&state->exceptionsCount,
				[=](uint64 id) { state->storage->addException(id); },
				[=](uint64 id) { state->storage->removeException(id); });
			listController->setStyleOverrides(&st::settingsBlockedList);
			exceptionsController = listController.get();

			builder.addButton({
				.id = u"exc/save_deleted/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.onClick = [=] {
					auto pickerController = std::make_unique<
						ChooseRecipientBoxController>(
						ChooseRecipientArgs{
							.session = &controller->session(),
							.callback = [=](
									not_null<Data::Thread*> thread) {
								exceptionsController->addPeer(
									thread->peer());
							},
							.filter = [=](
									not_null<Data::Thread*> thread) {
								return !state->exceptions.contains(
									thread->peer());
							},
						});
					controller->show(Box<PeerListBox>(
						std::move(pickerController),
						[](not_null<PeerListBox*> box) {
							box->addButton(
								tr::lng_cancel(),
								[=] { box->closeBox(); });
						}));
				},
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});

			const auto content = inner->add(
				object_ptr<PeerListContent>(
					inner,
					listController.get()));

			const auto es = content->lifetime().make_state<
				ExceptionsState>();
			es->controller = std::move(listController);
			es->delegate = std::make_unique<
				PeerListContentDelegateSimple>();
			es->delegate->setContent(content);
			es->controller->setDelegate(es->delegate.get());
		} else {
			builder.addButton({
				.id = u"exc/save_deleted/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});
		}

		const auto botsToggle = builder.addButton({
			.id = u"exc/save_deleted/bots"_q,
			.title = rpl::single(u"Save in bots?"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->saveDeletedInBotsEnabled() : false),
			.keywords = { u"bots"_q, u"deleted"_q },
		});

		if (botsToggle && state) {
			botsToggle->toggledChanges(
			) | rpl::on_next([=](bool enabled) {
				state->storage->setSaveDeletedInBotsEnabled(enabled);
			}, botsToggle->lifetime());
		}
	}, toggle ? toggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(u"When enabled, messages deleted by other users will be preserved locally. Use Exceptions to exclude specific chats."_q));

	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Edit History"_q));

	const auto editToggle = builder.addButton({
		.id = u"exc/save_edits"_q,
		.title = rpl::single(u"Save edit history"_q),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(
			state ? state->storage->saveEditsEnabled() : false),
		.keywords = { u"edit"_q, u"history"_q, u"save"_q },
	});

	if (editToggle && state) {
		editToggle->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			state->storage->setSaveEditsEnabled(enabled);
		}, editToggle->lifetime());
	}

	builder.scope([&] {
		struct EditExceptionsState {
			std::unique_ptr<GenericExceptionsController> controller;
			std::unique_ptr<PeerListContentDelegateSimple> delegate;
		};

		const auto inner = builder.container();
		GenericExceptionsController *editExceptionsCtrl = nullptr;

		if (inner && controller && state) {
			auto listController = std::make_unique<GenericExceptionsController>(
				&controller->session(),
				&state->editExceptions,
				&state->editExceptionsCount,
				[=](uint64 id) { state->storage->addEditException(id); },
				[=](uint64 id) { state->storage->removeEditException(id); });
			listController->setStyleOverrides(&st::settingsBlockedList);
			editExceptionsCtrl = listController.get();

			builder.addButton({
				.id = u"exc/save_edits/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.onClick = [=] {
					auto pickerController = std::make_unique<
						ChooseRecipientBoxController>(
						ChooseRecipientArgs{
							.session = &controller->session(),
							.callback = [=](
									not_null<Data::Thread*> thread) {
								editExceptionsCtrl->addPeer(
									thread->peer());
							},
							.filter = [=](
									not_null<Data::Thread*> thread) {
								return !state->editExceptions.contains(
									thread->peer());
							},
						});
					controller->show(Box<PeerListBox>(
						std::move(pickerController),
						[](not_null<PeerListBox*> box) {
							box->addButton(
								tr::lng_cancel(),
								[=] { box->closeBox(); });
						}));
				},
				.keywords = { u"exceptions"_q, u"add"_q },
			});

			const auto content = inner->add(
				object_ptr<PeerListContent>(
					inner,
					listController.get()));

			const auto es = content->lifetime().make_state<
				EditExceptionsState>();
			es->controller = std::move(listController);
			es->delegate = std::make_unique<
				PeerListContentDelegateSimple>();
			es->delegate->setContent(content);
			es->controller->setDelegate(es->delegate.get());
		} else {
			builder.addButton({
				.id = u"exc/save_edits/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.keywords = { u"exceptions"_q, u"add"_q },
			});
		}
	}, editToggle ? editToggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(u"When enabled, previous versions of edited messages will be preserved locally. Use Exceptions to exclude specific chats."_q));
}

void BuildGhostModeSection(
		SectionBuilder &builder,
		ProState *state) {
	const auto container = builder.container();
	const auto controller = builder.controller();

	builder.addSkip();
	builder.addSubsectionTitle(rpl::single(u"Ghost Mode"_q));

	struct GhostUIState {
		rpl::variable<int> enabledCount = 0;
	};
	const auto ghost = container
		? container->lifetime().make_state<GhostUIState>()
		: nullptr;

	const auto toggle = builder.addButton({
		.id = u"exc/ghost_mode"_q,
		.title = ghost
			? ghost->enabledCount.value(
			) | rpl::map([](int count) {
				return u"Ghost Mode (%1/5)"_q.arg(count);
			}) | rpl::type_erased
			: rpl::single(u"Ghost Mode (0/5)"_q) | rpl::type_erased,
		.icon = { &st::menuIconLock },
		.toggled = rpl::single(
			state ? state->storage->ghostEnabled() : false),
		.keywords = { u"ghost"_q, u"invisible"_q, u"privacy"_q },
	});

	if (toggle && state) {
		toggle->toggledChanges(
		) | rpl::on_next([=](bool enabled) {
			state->storage->setGhostEnabled(enabled);
		}, toggle->lifetime());
	}

	builder.scope([&] {
		struct SubToggles {
			Ui::SettingsButton *readReceipts = nullptr;
			Ui::SettingsButton *online = nullptr;
			Ui::SettingsButton *typing = nullptr;
			Ui::SettingsButton *readOnInteract = nullptr;
			Ui::SettingsButton *instantOnline = nullptr;
		};
		const auto subs = container
			? container->lifetime().make_state<SubToggles>()
			: nullptr;

		subs->readReceipts = builder.addButton({
			.id = u"exc/ghost/no_read"_q,
			.title = rpl::single(u"Don't send read receipts"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->ghostNoRead() : false),
			.keywords = { u"read"_q, u"receipts"_q, u"ghost"_q },
		});

		subs->online = builder.addButton({
			.id = u"exc/ghost/no_online"_q,
			.title = rpl::single(u"Don't update online status"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->ghostNoOnline() : false),
			.keywords = { u"online"_q, u"status"_q, u"ghost"_q },
		});

		subs->typing = builder.addButton({
			.id = u"exc/ghost/no_typing"_q,
			.title = rpl::single(u"Hide typing status"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->ghostNoTyping() : false),
			.keywords = { u"typing"_q, u"ghost"_q },
		});

		subs->readOnInteract = builder.addButton({
			.id = u"exc/ghost/read_on_interact"_q,
			.title = rpl::single(u"Mark read on interaction"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->ghostReadOnInteract() : false),
			.keywords = { u"read"_q, u"interact"_q, u"ghost"_q },
		});

		subs->instantOnline = builder.addButton({
			.id = u"exc/ghost/instant_online"_q,
			.title = rpl::single(u"Instant online after offline"_q),
			.st = &st::settingsButtonNoIcon,
			.toggled = rpl::single(
				state ? state->storage->ghostInstantOnline() : false),
			.keywords = { u"instant"_q, u"online"_q, u"ghost"_q },
		});

		if (state && subs->readReceipts) {
			subs->readReceipts->toggledChanges(
			) | rpl::on_next([=](bool v) {
				state->storage->setGhostNoRead(v);
			}, subs->readReceipts->lifetime());

			subs->online->toggledChanges(
			) | rpl::on_next([=](bool v) {
				state->storage->setGhostNoOnline(v);
			}, subs->online->lifetime());

			subs->typing->toggledChanges(
			) | rpl::on_next([=](bool v) {
				state->storage->setGhostNoTyping(v);
			}, subs->typing->lifetime());

			subs->readOnInteract->toggledChanges(
			) | rpl::on_next([=](bool v) {
				state->storage->setGhostReadOnInteract(v);
			}, subs->readOnInteract->lifetime());

			subs->instantOnline->toggledChanges(
			) | rpl::on_next([=](bool v) {
				state->storage->setGhostInstantOnline(v);
			}, subs->instantOnline->lifetime());
		}

		if (ghost && subs->readReceipts) {
			rpl::combine(
				subs->readReceipts->toggledValue(),
				subs->online->toggledValue(),
				subs->typing->toggledValue(),
				subs->readOnInteract->toggledValue(),
				subs->instantOnline->toggledValue()
			) | rpl::map([](bool a, bool b, bool c, bool d, bool e) {
				return int(a) + int(b) + int(c) + int(d) + int(e);
			}) | rpl::on_next([=](int count) {
				ghost->enabledCount = count;
			}, container->lifetime());
		}

		struct ExceptionsState {
			std::unique_ptr<GenericExceptionsController> controller;
			std::unique_ptr<PeerListContentDelegateSimple> delegate;
		};

		const auto inner = builder.container();
		GenericExceptionsController *exceptionsController = nullptr;

		if (inner && controller && state) {
			auto listController = std::make_unique<
				GenericExceptionsController>(
				&controller->session(),
				&state->ghostExceptions,
				&state->ghostExceptionsCount,
				[=](uint64 id) {
					state->storage->addGhostException(id);
				},
				[=](uint64 id) {
					state->storage->removeGhostException(id);
				});
			listController->setStyleOverrides(&st::settingsBlockedList);
			exceptionsController = listController.get();

			builder.addButton({
				.id = u"exc/ghost/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.onClick = [=] {
					auto pickerController = std::make_unique<
						ChooseRecipientBoxController>(
						ChooseRecipientArgs{
							.session = &controller->session(),
							.callback = [=](
									not_null<Data::Thread*> thread) {
								exceptionsController->addPeer(
									thread->peer());
							},
							.filter = [=](
									not_null<Data::Thread*> thread) {
								return !state->ghostExceptions.contains(
									thread->peer());
							},
						});
					controller->show(Box<PeerListBox>(
						std::move(pickerController),
						[](not_null<PeerListBox*> box) {
							box->addButton(
								tr::lng_cancel(),
								[=] { box->closeBox(); });
						}));
				},
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});

			const auto content = inner->add(
				object_ptr<PeerListContent>(
					inner,
					listController.get()));

			const auto es = content->lifetime().make_state<
				ExceptionsState>();
			es->controller = std::move(listController);
			es->delegate = std::make_unique<
				PeerListContentDelegateSimple>();
			es->delegate->setContent(content);
			es->controller->setDelegate(es->delegate.get());
		} else {
			builder.addButton({
				.id = u"exc/ghost/add_exception"_q,
				.title = rpl::single(u"Add exception"_q),
				.icon = { &st::menuIconInviteSettings },
				.keywords = { u"exceptions"_q, u"add"_q, u"chats"_q },
			});
		}
	}, toggle ? toggle->toggledValue() : nullptr);

	builder.addDividerText(rpl::single(
		u"Ghost Mode hides your online presence. Enable individual "
		"options to control which signals are suppressed. Use Exceptions "
		"to exclude specific chats from Ghost Mode."_q));
}

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
	const auto container = builder.container();
	const auto session = builder.session();
	const auto state = container
		? container->lifetime().make_state<ProState>()
		: nullptr;
	if (state) {
		InitProState(state, session);
	}
	BuildSaveDeletedSection(builder, state);
	BuildGhostModeSection(builder, state);

	builder.addDivider();
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
			if (const auto c = builder.controller()) {
				c->showToast({
					.text = { tr::lng_settings_exc_check_updates_mac_only(tr::now) },
				});
			}
#endif // Q_OS_MAC
		},
		.keywords = { u"update"_q, u"check"_q, u"sparkle"_q, u"exc"_q },
	});

	builder.addSkip();
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
