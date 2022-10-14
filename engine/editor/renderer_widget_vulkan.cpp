/*
 * Ham World Engine Editor
 * Copyright (C) 2022 Hamsmith Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "renderer_widget.hpp"

#include <QWindow>
#include <QVulkanInstance>
#include <QVulkanFunctions>

#include <QBoxLayout>

using namespace ham::typedefs;

namespace editor = ham::engine::editor;

static thread_local QVulkanInstance *ham_impl_vulkan_inst = nullptr;

editor::renderer_widget_vulkan::renderer_widget_vulkan(QWidget *parent)
	: renderer_widget(parent)
	, m_inst(new QVulkanInstance)
{
	#ifdef HAM_DEBUG
	m_inst->setLayers(QByteArrayList() << "VK_LAYER_KHRONOS_validation");
	#endif

	if(!m_inst->create()){
		qFatal("Failed to create QVulkanInstance: %s", ham_vk_result_str(m_inst->errorCode()));
	}

	const auto win = new QWindow();
	const auto container = QWidget::createWindowContainer(win);

	win->setSurfaceType(QSurface::VulkanSurface);
	win->show();
	win->setVulkanInstance(m_inst);

	const auto surface = m_inst->surfaceForWindow(win);

	container->setContentsMargins(0, 0, 0, 0);

	const auto lay = new QBoxLayout(QBoxLayout::LeftToRight, this);

	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(container);

	setLayout(lay);

	const auto vkGetInstanceProcAddr_ptr = (PFN_vkGetInstanceProcAddr)m_inst->getInstanceProcAddr("vkGetInstanceProcAddr");
	if(!vkGetInstanceProcAddr_ptr){
		qFatal("Failed to get vkGetInstanceProcAddr");
	}

	ham_impl_vulkan_inst = m_inst;

	ham_renderer_create_args create_args;
	create_args.vulkan = {
		.instance = m_inst->vkInstance(),
		.surface  = surface,
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr_ptr,
	};

	if(!initialize_renderer(HAM_RENDERER_VULKAN_PLUGIN_NAME, HAM_RENDERER_VULKAN_OBJECT_NAME, &create_args)){
		qFatal("Error initializing renderer");
	}

	resize_renderer(width(), height());

	const auto resize_handler = [this](){
		resize_renderer(width(), height());
	};

	connect(win, &QWindow::widthChanged, this, resize_handler);
	connect(win, &QWindow::heightChanged, this, resize_handler);
}

editor::renderer_widget_vulkan::~renderer_widget_vulkan(){}

void editor::renderer_widget_vulkan::paintEvent(QPaintEvent *ev){
	renderer_widget::paintEvent(ev);
	paint_renderer();
}
