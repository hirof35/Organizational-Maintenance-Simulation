# include <Siv3D.hpp>

struct Agent {
	Vec2 pos;
	Vec2 velocity;
	bool isAlive = true;
	bool isPanicked = false;
	bool isSpy = false;
	double panicTimer = 0.0;

	void update(bool isAutonomous, const Vec2& leaderPos, const Array<Agent>& agents,
				const Circle& enemy, size_t myIndex, double conformity, double criticalThinking) {
		if (!isAlive) return;

		if (isSpy) {
			isPanicked = true;
			velocity += RandomVec2(1.5);
		}
		else if (isPanicked) {
			velocity += RandomVec2(2.0);
			panicTimer += Scene::DeltaTime();
			if (panicTimer > 2.0) { isPanicked = false; panicTimer = 0; }
		}

		if (isAutonomous) {
			Vec2 separation(0, 0);
			Vec2 alignment(0, 0);
			int32 neighbors = 0;

			for (size_t i = 0; i < agents.size(); ++i) {
				if (i == myIndex || !agents[i].isAlive) continue;
				double dist = pos.distanceFrom(agents[i].pos);
				if (dist < 80.0) {
					double neighborSpeed = agents[i].velocity.length();
					if (neighborSpeed > (6.0 - criticalThinking * 4.0)) continue;
					if (agents[i].isPanicked && RandomBool(0.01 * conformity)) isPanicked = true;
					separation += (pos - agents[i].pos).setLength(1.5 / (dist + 1.0));
					alignment += agents[i].velocity;
					neighbors++;
				}
			}
			if (neighbors > 0) velocity += (separation * 2.0) + (alignment / neighbors * 0.2 * conformity);
			if (pos.distanceFrom(enemy.center) < 150.0) velocity += (pos - enemy.center).setLength(0.8);
		}
		else {
			velocity += (leaderPos - pos).setLength(0.5);
		}

		velocity += (Scene::Center() - pos) * 0.002;
		velocity *= 0.95;
		pos += velocity;
		if (pos.intersects(enemy)) isAlive = false;
	}

	void draw(bool isAutonomous) const {
		if (!isAlive) return;
		ColorF color = isSpy ? Palette::Red : (isPanicked ? Palette::Orange : (isAutonomous ? Palette::Limegreen : Palette::Skyblue));
		Circle{ pos, 8 }.draw(color).drawFrame(1, 0, Palette::White);
		if (isSpy) Circle{ pos, 12 }.drawFrame(2, Palette::Red);
	}
};

void Main() {
	Window::Resize(1280, 720);
	const Font font{ 20, Typeface::Medium };
	Array<Agent> agents;
	bool isAutonomous = true;
	double conformity = 1.0;
	double criticalThinking = 0.5;
	Circle enemy{ Scene::Center(), 60 };
	CSV csv;
	csv.writeRow(U"Time", U"Healthy", U"Panic", U"Dead", U"Mode", U"CriticalThinking");

	auto Reset = [&]() {
		agents.clear();
		for (int i = 0; i < 60; ++i) agents << Agent{ RandomVec2(Scene::Rect()), Vec2{0,0} };
		for (int i = 0; i < 5; ++i) agents[i].isSpy = true;
		};
	Reset();

	Stopwatch dataTimer{ StartImmediately::Yes };

	while (System::Update()) {
		// --- 1. 更新処理 ---
		enemy.center = Cursor::Pos();
		Vec2 leaderPos = Circular{ 250, Scene::Time() * 0.6 }.toVec2() + Scene::Center();
		for (size_t i = 0; i < agents.size(); ++i) {
			agents[i].update(isAutonomous, leaderPos, agents, enemy, i, conformity, criticalThinking);
		}

		// --- 2. 描画処理 ---
		for (const auto& agent : agents) agent.draw(isAutonomous);
		enemy.draw(ColorF(1, 0, 0, 0.2)).drawFrame(2, Palette::Red);
		if (!isAutonomous) Circle{ leaderPos, 12 }.drawFrame(2, Palette::White);

		// --- 3. UIレイアウト (右側に配置) ---
		const double uiX = Scene::Width() - 220;
		Rect{ (int)uiX - 10, 0, 230, Scene::Height() }.draw(ColorF(0.1, 0.8)); // 背景

		SimpleGUI::CheckBox(isAutonomous, U"Autonomous", Vec2{ uiX, 20 });
		font(U"Conformity").draw(uiX, 60);
		SimpleGUI::Slider(conformity, 0.0, 2.0, Vec2{ uiX, 90 }, 180);
		font(U"Critical Thinking").draw(uiX, 130);
		SimpleGUI::Slider(criticalThinking, 0.0, 1.0, Vec2{ uiX, 160 }, 180);

		if (SimpleGUI::Button(U"Reset (R key)", Vec2{ uiX, 210 }, 180)) Reset();
		if (SimpleGUI::Button(U"Save CSV", Vec2{ uiX, 260 }, 180)) {
			csv.save(U"org_sim_data.csv");
		}
		if (KeyR.down()) Reset();

		// --- 4. 統計情報の表示 (下部に配置) ---
		int32 dead = agents.count_if([](const Agent& a) { return !a.isAlive; });
		int32 panic = agents.count_if([](const Agent& a) { return a.isAlive && a.isPanicked && !a.isSpy; });
		int32 healthy = agents.count_if([](const Agent& a) { return a.isAlive && !a.isPanicked && !a.isSpy; });

		Rect{ 0, Scene::Height() - 60, (int)uiX - 10, 60 }.draw(ColorF(0, 0.7));
		font(U"Healthy: {}  |  Panic: {}  |  Dead: {}"_fmt(healthy, panic, dead))
			.drawAt(Scene::Center().x - 110, Scene::Height() - 30);

		if (dataTimer.elapsed() >= 1.0s) {
			csv.writeRow(Scene::Time(), healthy, panic, dead, isAutonomous ? 1 : 0, criticalThinking);
			dataTimer.restart();
		}
	}
}
