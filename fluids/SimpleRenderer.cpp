#include "SimpleRenderer.h"
#include "Input.h"

#include <cstdlib>

#include <GLFW\glfw3.h>
#include <nanogui\nanogui.h>
#include <glm/common.hpp>
#include <glm/gtx/rotate_vector.hpp>


void SimpleRenderer::init(const glm::vec3 &cam_pos, const glm::vec3 &cam_focus) {

	m_width = WINDOW_WIDTH;
	m_height = WINDOW_HEIGHT;

	glfwInit();
	glfwSetTime(0);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_SAMPLES, 0);
	glfwWindowHint(GLFW_RED_BITS, 8);
	glfwWindowHint(GLFW_GREEN_BITS, 8);
	glfwWindowHint(GLFW_BLUE_BITS, 8);
	glfwWindowHint(GLFW_ALPHA_BITS, 8);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	m_input = new Input();

	m_window = glfwCreateWindow(m_width, m_height, "Fluid", nullptr, nullptr);
	if (m_window == nullptr) {
		printf("Failed to create GLFW window\n");
		glfwTerminate();
		fexit(-1);
	}
	glfwMakeContextCurrent(m_window);

	if (!gladLoadGL()) {
		printf("Failed to initialize GLAD\n");
		fexit(-1);
	}

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		throw std::runtime_error("Could not initialize GLAD!");
	glGetError();

	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	// glEnable(GL_BLEND);
	// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	/* Init nanogui */
	// m_gui_screen = new nanogui::Screen(Eigen::Vector2i(1024, 768), "Fluids");
	m_gui_screen = new nanogui::Screen();
	m_gui_screen->initialize(m_window, true);
	m_gui_screen->setSize(Eigen::Vector2i(800, 600));

	int width_, height_;
	glfwGetFramebufferSize(m_window, &width_, &height_);
	glViewport(0, 0, width_, height_);
	glfwSwapInterval(0);
	glfwSwapBuffers(m_window);

	m_gui_form = new nanogui::FormHelper(m_gui_screen);
	nanogui::ref<nanogui::Window> nanoWin = m_gui_form->addWindow(Eigen::Vector2i(30, 50), "Parameters");

	GUIParams &params = GUIParams::getInstance();
	m_gui_form->addVariable("# Frame", m_input->frameCount)->setEditable(false);
	m_gui_form->addVariable("# Iter", params.niter)->setSpinnable(true);
	m_gui_form->addVariable("pho0", params.pho0)->setSpinnable(true);
	m_gui_form->addVariable("g", params.g)->setSpinnable(true);
	m_gui_form->addVariable("h", params.h)->setSpinnable(true);
	m_gui_form->addVariable("dt", params.dt)->setSpinnable(true);
	m_gui_form->addVariable("lambda_eps", params.lambda_eps)->setSpinnable(true);
	m_gui_form->addVariable("delta_q", params.delta_q)->setSpinnable(true);
	m_gui_form->addVariable("k_corr", params.k_corr)->setSpinnable(true);
	m_gui_form->addVariable("n_corr", params.n_corr)->setSpinnable(true);
	m_gui_form->addVariable("k_boundary", params.k_boundaryDensity)->setSpinnable(true);
	m_gui_form->addVariable("c_XSPH", params.c_XSPH)->setSpinnable(true);
	m_gui_form->addVariable("Highlight #", m_input->hlIndex)->setSpinnable(true);
	
	auto smooth_niter = m_gui_form->addVariable("Smooth # Iter", params.smooth_niter); 
	smooth_niter->setMinMaxValues(0, 60);
	smooth_niter->setSpinnable(true);

	auto kernel_r = m_gui_form->addVariable("kernel_r", params.kernel_r);
	kernel_r->setMinMaxValues(0, 20);
	kernel_r->setSpinnable(true);

	auto sigma_r = m_gui_form->addVariable("sigma_r", params.sigma_r);
	sigma_r->setMinMaxValues(0.f, 10.f);
	sigma_r->setSpinnable(true);

	auto sigma_z = m_gui_form->addVariable("sigma_z", params.sigma_z);
	sigma_z->setMinMaxValues(0.f, 1.f);
	sigma_z->setSpinnable(true);

	m_gui_form->addButton("Next Frame", [this]() { m_nextFrameBtnCb();  });
	auto runBtn = m_gui_form->addButton("Run", []() {});
	runBtn->setFlags(nanogui::Button::ToggleButton);
	runBtn->setChangeCallback([this](bool state) { m_input->running = state; });
	auto lastFrameBtn = m_gui_form->addButton("Last Frame", []() {});
	lastFrameBtn->setFlags(nanogui::Button::ToggleButton);
	lastFrameBtn->setChangeCallback([this](bool state) { m_input->lastFrame = state; });

	m_gui_screen->setVisible(true);
	m_gui_screen->performLayout();
	// nanoWin->center();

	__binding();

	/* Resource allocation in constructor */
	float aspect = (float)WINDOW_WIDTH / WINDOW_HEIGHT;

	m_camera = new Camera(cam_pos, cam_focus, aspect);
	/* This will loaded shader from shader/simple.cpp automatically */
	m_box_shader = new Shader(box_vshader, box_fshader);
	m_particle_shader = new Shader(Filename("vertex.glsl"), Filename("fragment.glsl"));

	/* SSFRenderer */
	m_SSFrenderer = new SSFRenderer(m_camera, width_, height_);
	printf("new SSFRenderer()\n");

	glGenVertexArrays(1, &d_vao);
	glGenVertexArrays(1, &d_bbox_vao);

	glGenBuffers(1, &d_bbox_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, d_bbox_vbo);
	glBufferData(GL_ARRAY_BUFFER, 12 * 2 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindVertexArray(d_bbox_vao);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
}

void SimpleRenderer::__window_size_callback(GLFWwindow* window, int width, int height) {
	m_width = width;
	m_height = height;
	glViewport(0, 0, width, height);
	m_camera->setAspect((float)width / height);
	m_gui_screen->resizeCallbackEvent(width, height);
}

void SimpleRenderer::__mouse_button_callback(GLFWwindow *w, int button, int action, int mods) {
	if (m_gui_screen->mouseButtonCallbackEvent(button, action, mods)) return;

	Input::Pressed updown = action == GLFW_PRESS ? Input::DOWN : Input::UP;
	if (button == GLFW_MOUSE_BUTTON_LEFT)
		m_input->left_mouse = updown;
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
		m_input->right_mouse = updown;
	if (button == GLFW_MOUSE_BUTTON_MIDDLE)
		m_input->mid_mouse = updown;
}

void SimpleRenderer::__mouse_move_callback(GLFWwindow* window, double xpos, double ypos) {
	if (m_gui_screen->cursorPosCallbackEvent(xpos, ypos)) return;

	m_input->updateMousePos(glm::vec2(xpos, ypos));

	/* -- Camera control -- */

	/* Rotating */
	glm::vec2 scr_d = m_input->getMouseDiff();
	glm::vec3 pos = m_camera->getPos(), front = m_camera->getFront(), center = pos + front, up = m_camera->getUp();
	glm::vec3 cam_d = scr_d.x * -glm::normalize(glm::cross(front, up)) + scr_d.y * glm::normalize(up);

	if (m_input->left_mouse == Input::DOWN)
		m_camera->rotate(scr_d);

	/* Panning */
	if (m_input->right_mouse == Input::DOWN)
		m_camera->pan(scr_d);
	
}

void SimpleRenderer::__key_callback(GLFWwindow *w, int key, int scancode, int action, int mods) {
	m_gui_screen->keyCallbackEvent(key, scancode, action, mods);
}

void SimpleRenderer::__mouse_scroll_callback(GLFWwindow *w, float dx, float dy) {
	if(m_gui_screen->scrollCallbackEvent(dx, dy)) return;
	m_camera->zoom(dy);
}

void SimpleRenderer::__char_callback(GLFWwindow *w, unsigned int codepoint) {
	m_gui_screen->charCallbackEvent(codepoint);
}

void SimpleRenderer::__binding() {
	// glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	/* glfw input callback requires static function (not class method) 
	 * as a workaround, glfw provides a 'user pointer', 
	 * by which static function gets access to class method
	 */
	glfwSetWindowUserPointer(m_window, this);

	/* Windows resize */
	glfwSetWindowSizeCallback(m_window, [](GLFWwindow *win, int width, int height) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(win)))->__window_size_callback(win, width, height);
	});

	/* Mouse move */
	glfwSetCursorPosCallback(m_window, [](GLFWwindow *w, double xpos, double ypos) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(w)))->__mouse_move_callback(w, xpos, ypos);
	});

	/* Mouse Button */
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int button, int action, int mods) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(w)))->__mouse_button_callback(w, button, action, mods);
	});

	/* Mouse Scroll */
	glfwSetScrollCallback(m_window, [](GLFWwindow *w, double dx, double dy) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(w)))->__mouse_scroll_callback(w, dx, dy);
	});

	/* GUI keyboard input */
	glfwSetKeyCallback(m_window,
		[](GLFWwindow *w, int key, int scancode, int action, int mods) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(w)))->__key_callback(w, key, scancode, action, mods);
	});

	glfwSetCharCallback(m_window,
		[](GLFWwindow *w, unsigned int codepoint) {
		((SimpleRenderer*)(glfwGetWindowUserPointer(w)))->__char_callback(w, codepoint);
	});
}

bool move = false;
void SimpleRenderer::__processInput() {
	if (glfwGetKey(m_window, GLFW_KEY_M) == GLFW_PRESS)
		move = true;
}

void SimpleRenderer::__render() {
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*if (m_particle_shader->loaded()) {
		m_particle_shader->use();
		m_camera->use(Shader::now());

		m_particle_shader->setUnif("color", glm::vec4(1.f, 0.f, 0.f, .1f));
		m_particle_shader->setUnif("pointRadius", m_input->fluidParams.h);
		m_particle_shader->setUnif("pointScale", 500.f);
		m_particle_shader->setUnif("hlIndex", m_input->hlIndex);
		glBindVertexArray(d_vao);
		glDrawArrays(GL_POINTS, 0, m_nparticle);
	}*/

	m_SSFrenderer->render(d_vao, m_nparticle);

	if (m_box_shader->loaded()) {
		m_box_shader->use();
		m_camera->use(Shader::now());

		/* draw particles */
		glBindVertexArray(d_bbox_vao);
		m_box_shader->setUnif("color", glm::vec4(1.f, 1.f, 1.f, 1.f));
		glDrawArrays(GL_LINES, 0, 12 * 2);
	}

}

SimpleRenderer::~SimpleRenderer()
{
	if (m_camera) delete m_camera;
	if (m_box_shader) delete m_box_shader;
	if (m_particle_shader) delete m_particle_shader;
	/* TODO: m_window, input */
	if (m_input) delete m_input;
}

void SimpleRenderer::render(uint pos, uint iid, int nparticle) {
	/** 
	 * @input pos vertex buffer object 
	 */
	d_iid = iid;
	d_pos = pos;
	m_nparticle = nparticle;

	glBindVertexArray(d_vao);
	glBindBuffer(GL_ARRAY_BUFFER, d_pos);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, d_iid);
	/* MUST use glVertexAttribIPointer, not glVertexAttribPointer, for uint attribute */
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 0, (void*)0);
	glEnableVertexAttribArray(1);

	/* == Bounding box == */
	float x1 = fmin(m_ulim.x, m_llim.x), x2 = fmax(m_ulim.x, m_llim.x),
		y1 = fmin(m_ulim.y, m_llim.y), y2 = fmax(m_ulim.y, m_llim.y),
		z1 = fmin(m_ulim.z, m_llim.z), z2 = fmax(m_ulim.z, m_llim.z);

	glm::vec3 lines[][2] = {
		{ glm::vec3(x1, y1, z1), glm::vec3(x2, y1, z1) },
		{ glm::vec3(x1, y1, z2), glm::vec3(x2, y1, z2) },
		{ glm::vec3(x1, y2, z1), glm::vec3(x2, y2, z1) },
		{ glm::vec3(x1, y2, z2), glm::vec3(x2, y2, z2) },

		{ glm::vec3(x1, y1, z1), glm::vec3(x1, y2, z1) },
		{ glm::vec3(x1, y1, z2), glm::vec3(x1, y2, z2) },
		{ glm::vec3(x2, y1, z1), glm::vec3(x2, y2, z1) },
		{ glm::vec3(x2, y1, z2), glm::vec3(x2, y2, z2) },

		{ glm::vec3(x1, y1, z1), glm::vec3(x1, y1, z2) },
		{ glm::vec3(x1, y2, z1), glm::vec3(x1, y2, z2) },
		{ glm::vec3(x2, y1, z1), glm::vec3(x2, y1, z2) },
		{ glm::vec3(x2, y2, z1), glm::vec3(x2, y2, z2) } };
	glBindBuffer(GL_ARRAY_BUFFER, d_bbox_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), lines);

	m_gui_form->refresh();

	if (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();
		__processInput();
		__render();
		m_gui_screen->drawContents();
		m_gui_screen->drawWidgets();
		glfwSwapBuffers(m_window);
	}
	else fexit(0);
}